#include "SAABB.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"
#include "Core/SceneGraph/Mesh.h"
#include "SimpleMath/SimpleMath.h"

void ECS::System::SAABB::Init(const SystemInitData& data)
{
}

void ECS::System::SAABB::RebuildRenderableList()
{
    m_renderableEntries.clear();

    const auto& transComps = g_pEntityManager->GetComponentVector<Components::Transform>();
    const auto& meshAABBs  = g_pMeshManager->GetMeshAABBs();

    for (const auto& holder : transComps)
    {
        Components::RenderComponent* pRenderComp = nullptr;

        if (g_pEntityManager->HasComponent<Components::RenderComponent>(holder.entity))
            pRenderComp = g_pEntityManager->GetComponentUnsafe<Components::RenderComponent>(holder.entity);
        else if (g_pEntityManager->HasComponent<Components::DebugRenderComponent>(holder.entity))
            pRenderComp = static_cast<Components::RenderComponent*>(
                g_pEntityManager->GetComponentUnsafe<Components::DebugRenderComponent>(holder.entity));
        else
            continue;

        auto meshIt = meshAABBs.find(pRenderComp->pMesh);
        if (meshIt == meshAABBs.end())
            continue;

        RenderableEntry entry;
        entry.pTransform   = &holder.component;
        entry.pRenderComp  = pRenderComp;
        entry.meshExtents  = meshIt->second.extents;
        m_renderableEntries.push_back(entry);
    }
}

void ECS::System::SAABB::Process()
{
    ScopedZone("AABB System::Process");

    if (g_pMeshManager->GetMeshAABBs().empty())
        return;

    const size_t currentTransformCount =
        g_pEntityManager->GetComponentVector<Components::Transform>().size();

    if (currentTransformCount != m_lastKnownTransformCount)
    {
        m_lastKnownTransformCount = currentTransformCount;
        RebuildRenderableList();
    }

    if (m_renderableEntries.empty())
        return;

    const bool allDirty = g_pEntityManager->IsAllTransformsDirty() ||
                          g_pEntityManager->GetDirtyEntities(C_ID(Transform)).empty();
    const auto& updatedTransforms = g_pEntityManager->GetTransformsUpdatedThisFrame();

    if (allDirty)
    {
        // Full scene update: iterate pre-filtered list with no hash lookups
        for (const auto& entry : m_renderableEntries)
        {
            entry.pRenderComp->boundingBox.center = mathstl::Vector4(entry.pTransform->worldPosition.x,
                                                                     entry.pTransform->worldPosition.y,
                                                                     entry.pTransform->worldPosition.z,
                                                                     0.0f);
            entry.pRenderComp->boundingBox.extents = entry.meshExtents * entry.pTransform->worldScale;
        }
        return;
    }

    // STransform already ran and populated updatedTransforms with every entity whose
    // world matrix actually changed this frame. Check if any of them are renderable.
    bool anyRenderableUpdated = false;
    for (const Entity& entity : updatedTransforms)
    {
        if (g_pEntityManager->HasComponent<Components::RenderComponent>(entity) ||
            g_pEntityManager->HasComponent<Components::DebugRenderComponent>(entity))
        {
            anyRenderableUpdated = true;
            break;
        }
    }

    if (!anyRenderableUpdated)
        return;

    // Update only the transforms that actually changed and are renderable
    for (const Entity& entity : updatedTransforms)
    {
        Components::RenderComponent* pRenderComp = nullptr;

        if (g_pEntityManager->HasComponent<Components::RenderComponent>(entity))
            pRenderComp = g_pEntityManager->GetComponentUnsafe<Components::RenderComponent>(entity);
        else if (g_pEntityManager->HasComponent<Components::DebugRenderComponent>(entity))
            pRenderComp = static_cast<Components::RenderComponent*>(
                g_pEntityManager->GetComponentUnsafe<Components::DebugRenderComponent>(entity));
        else
            continue;

        const auto* pTransform = g_pEntityManager->GetComponentUnsafe<Components::Transform>(entity);
        const auto& meshAABBs  = g_pMeshManager->GetMeshAABBs();
        auto meshIt = meshAABBs.find(pRenderComp->pMesh);
        if (meshIt == meshAABBs.end())
            continue;

        pRenderComp->boundingBox.center = mathstl::Vector4(pTransform->worldPosition.x,
                                                           pTransform->worldPosition.y,
                                                           pTransform->worldPosition.z,
                                                           0.0f);
        pRenderComp->boundingBox.extents = meshIt->second.extents * pTransform->worldScale;
    }
}

void ECS::System::SAABB::SyncData(u32 currentFrame)
{
}

bool ECS::System::SAABB::AccessesAnyComponents(const stltype::vector<C_ID>& components)
{
    return AccessesComponent<ECS::Components::Transform>(components);
}
