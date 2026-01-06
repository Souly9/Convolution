#include "SAABB.h"
#include "Core/ECS/EntityManager.h"
#include "Core/SceneGraph/Mesh.h"

void ECS::System::SAABB::Init(const SystemInitData& data)
{
}

void ECS::System::SAABB::Process()
{
    ScopedZone("AABB System::Process");
    auto& transComps = g_pEntityManager->GetComponentVector<Components::Transform>();
    const auto& meshAABBs = g_pMeshManager->GetMeshAABBs();

    if (meshAABBs.empty())
        return;

    for (const auto& transform : transComps)
    {
        const auto& transformComp = transform.component;

        Components::RenderComponent* pRenderComp = nullptr;
        if (g_pEntityManager->HasComponent<Components::RenderComponent>(transform.entity))
            pRenderComp = g_pEntityManager->GetComponentUnsafe<Components::RenderComponent>(transform.entity);
        else if (g_pEntityManager->HasComponent<Components::DebugRenderComponent>(transform.entity))
            pRenderComp = static_cast<Components::RenderComponent*>(
                g_pEntityManager->GetComponentUnsafe<Components::DebugRenderComponent>(transform.entity));
        else
            continue;
        pRenderComp->boundingBox.center = transformComp.worldPosition;
        const auto& meshAABB = meshAABBs.find(pRenderComp->pMesh)->second;
        pRenderComp->boundingBox.extents = meshAABB.extents * transformComp.worldScale;
    }
}

void ECS::System::SAABB::SyncData(u32 currentFrame)
{
}

bool ECS::System::SAABB::AccessesAnyComponents(const stltype::vector<C_ID>& components)
{
    return AccessesComponent<ECS::Components::Transform>(components);
}
