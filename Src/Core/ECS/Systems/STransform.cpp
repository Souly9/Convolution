#include "STransform.h"
#include "Core/ECS/EntityManager.h"

using namespace DirectX;

void ECS::System::STransform::Init(const SystemInitData& data)
{
    m_pPassManager = data.pPassManager;
    m_cachedDataMap.reserve(2048);
}

void ECS::System::STransform::RebuildHierarchy(
    stltype::vector<ComponentHolder<Components::Transform>>& transComps)
{
    ScopedZone("RebuildHierarchy");
    for (auto& holder : transComps)
        holder.component.children.clear();

    for (auto& holder : transComps)
    {
        if (holder.component.HasParent())
        {
            Components::Transform* pParent =
                g_pEntityManager->GetComponentUnsafe<Components::Transform>(holder.component.parent);
            if (pParent)
                pParent->children.push_back(holder.entity);
        }
    }
}

void ECS::System::STransform::UpdateNode(Entity entity, const mathstl::Matrix& parentWorld, bool dirty)
{
    ECS::Components::Transform* pTransform =
        g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(entity);
    dirty = dirty | pTransform->isDirty;

    if (dirty)
    {
        pTransform->localModelMatrix = ComputeModelMatrix(pTransform);
        const mathstl::Matrix& world =
            pTransform->HasParent()
                ? (pTransform->worldModelMatrix = pTransform->localModelMatrix * parentWorld)
                : (pTransform->worldModelMatrix = pTransform->localModelMatrix);

        pTransform->worldPosition = mathstl::Vector3(world._41, world._42, world._43);
        pTransform->worldScale = mathstl::Vector3(
            sqrtf(world._11 * world._11 + world._12 * world._12 + world._13 * world._13),
            sqrtf(world._21 * world._21 + world._22 * world._22 + world._23 * world._23),
            sqrtf(world._31 * world._31 + world._32 * world._32 + world._33 * world._33));

        m_cachedDataMap.push_back({entity.ID, world});
        g_pEntityManager->NotifyTransformUpdated(entity);
        pTransform->isDirty = false;
    }

    for (const Entity& child : pTransform->children)
        UpdateNode(child, pTransform->worldModelMatrix, dirty);
}

void ECS::System::STransform::Process()
{
    ScopedZone("Transform System::Process");
    auto& transComps = g_pEntityManager->GetComponentVector<Components::Transform>();

    m_cachedDataMap.clear();

    if (transComps.size() != m_lastKnownTransformCount)
    {
        m_lastKnownTransformCount = transComps.size();
        RebuildHierarchy(transComps);
    }

    {
        ScopedZone("Update Transforms");

        const auto& dirtyEntities = g_pEntityManager->GetDirtyEntities(C_ID(Transform));
        const bool allDirty = g_pEntityManager->IsAllTransformsDirty() || dirtyEntities.empty();

        if (allDirty)
        {
            for (auto& holder : transComps)
            {
                if (!holder.component.HasParent())
                    UpdateNode(holder.entity, mathstl::Matrix::Identity, false);
            }
        }
        else
        {
            for (const Entity& entity : dirtyEntities)
            {
                auto* pTransform = g_pEntityManager->GetComponentUnsafe<Components::Transform>(entity);
                if (!pTransform)
                    continue;

                pTransform->isDirty = true;

                mathstl::Matrix parentWorld = mathstl::Matrix::Identity;
                if (pTransform->HasParent())
                {
                    auto* pParent = g_pEntityManager->GetComponentUnsafe<Components::Transform>(pTransform->parent);
                    parentWorld = pParent->worldModelMatrix;
                }

                UpdateNode(entity, parentWorld, true);
            }
        }
    }
}

void ECS::System::STransform::SyncData(u32 currentFrame)
{
    ScopedZone("Transform System::SyncData");
    m_pPassManager->SetEntityTransformDataForFrame(stltype::move(m_cachedDataMap), currentFrame);
}

bool ECS::System::STransform::AccessesAnyComponents(const stltype::vector<C_ID>& components)
{
    return stltype::find(components.begin(), components.end(), ComponentID<Components::Transform>::ID) !=
           components.end();
}

mathstl::Matrix ECS::System::STransform::ComputeModelMatrix(const ECS::Components::Transform* pTransform)
{
    using namespace mathstl;
    Vector3 rotation(pTransform->rotation);
    rotation *= XM_PI / 180.f;

    Quaternion transformQuat = Quaternion::CreateFromYawPitchRoll(rotation.y, rotation.x, rotation.z);

    return Matrix::CreateScale(pTransform->scale) * Matrix::CreateFromQuaternion(transformQuat) *
           Matrix::CreateTranslation(pTransform->position);
}
