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

    const auto& transComps = g_engine.GetEntityManager().GetComponentVector<Components::Transform>();
    const auto& meshAABBs  = g_engine.GetMeshManager().GetMeshAABBs();

    for (const auto& holder : transComps)
    {
        Components::RenderComponent* pRenderComp = nullptr;

        if (g_engine.GetEntityManager().HasComponent<Components::RenderComponent>(holder.entity))
            pRenderComp = g_engine.GetEntityManager().GetComponentUnsafe<Components::RenderComponent>(holder.entity);
        else if (g_engine.GetEntityManager().HasComponent<Components::DebugRenderComponent>(holder.entity))
            pRenderComp = static_cast<Components::RenderComponent*>(
                g_engine.GetEntityManager().GetComponentUnsafe<Components::DebugRenderComponent>(holder.entity));
        else
            continue;

        auto meshIt = meshAABBs.find(pRenderComp->pMesh);
        if (meshIt == meshAABBs.end())
            continue;

        RenderableEntry entry;
        entry.pTransform   = &holder.component;
        entry.pRenderComp  = pRenderComp;
        entry.localAABB    = meshIt->second;
        entry.pRenderComp->boundingBox = meshIt->second;
        m_renderableEntries.push_back(entry);
    }
}

void ECS::System::SAABB::Process()
{
    ScopedZone("AABB System::Process");

    if (g_engine.GetMeshManager().GetMeshAABBs().empty())
        return;

    const size_t currentTransformCount =
        g_engine.GetEntityManager().GetComponentVector<Components::Transform>().size();

    if (currentTransformCount != m_lastKnownTransformCount)
    {
        m_lastKnownTransformCount = currentTransformCount;
        RebuildRenderableList();
    }

    if (m_renderableEntries.empty())
        return;

    const bool allDirty = g_engine.GetEntityManager().IsAllTransformsDirty() ||
                          g_engine.GetEntityManager().GetDirtyEntities(C_ID(Transform)).empty();
    const auto& updatedTransforms = g_engine.GetEntityManager().GetTransformsUpdatedThisFrame();

    if (allDirty)
    {
        for (const auto& entry : m_renderableEntries)
        {
            entry.pRenderComp->boundingBox = entry.localAABB;
        }
        return;
    }

    bool anyRenderableUpdated = false;
    for (const Entity& entity : updatedTransforms)
    {
        if (g_engine.GetEntityManager().HasComponent<Components::RenderComponent>(entity) ||
            g_engine.GetEntityManager().HasComponent<Components::DebugRenderComponent>(entity))
        {
            anyRenderableUpdated = true;
            break;
        }
    }

    if (!anyRenderableUpdated)
        return;

    for (const Entity& entity : updatedTransforms)
    {
        Components::RenderComponent* pRenderComp = nullptr;

        if (g_engine.GetEntityManager().HasComponent<Components::RenderComponent>(entity))
            pRenderComp = g_engine.GetEntityManager().GetComponentUnsafe<Components::RenderComponent>(entity);
        else if (g_engine.GetEntityManager().HasComponent<Components::DebugRenderComponent>(entity))
            pRenderComp = static_cast<Components::RenderComponent*>(
                g_engine.GetEntityManager().GetComponentUnsafe<Components::DebugRenderComponent>(entity));
        else
            continue;

        const auto& meshAABBs = g_engine.GetMeshManager().GetMeshAABBs();
        auto meshIt = meshAABBs.find(pRenderComp->pMesh);
        if (meshIt == meshAABBs.end())
            continue;

        pRenderComp->boundingBox = meshIt->second;
    }
}

void ECS::System::SAABB::SyncData(u32 currentFrame)
{
}

bool ECS::System::SAABB::AccessesAnyComponents(const stltype::vector<C_ID>& components)
{
    return AccessesComponent<ECS::Components::Transform>(components);
}
