#include "SRenderComponent.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Core/SceneGraph/Mesh.h"

void ECS::System::SRenderComponent::Init(const SystemInitData& data)
{
    DEBUG_ASSERT(data.pPassManager);
    m_pPassManager = data.pPassManager;
}

void ECS::System::SRenderComponent::Process()
{
}

void ECS::System::SRenderComponent::SyncData(u32 currentFrame)
{
    ScopedZone("RenderComponent System::SyncData");
    const auto& renderComps = g_engine.GetEntityManager().GetComponentVector<Components::RenderComponent>();
    const auto& debugRenderComps = g_engine.GetEntityManager().GetComponentVector<Components::DebugRenderComponent>();
    const auto& meshAABBs = g_engine.GetMeshManager().GetMeshAABBs();

    RenderPasses::EntityMeshDataMap dataMap;
    dataMap.reserve(renderComps.size());

    const auto localAABBOf = [&meshAABBs](const Components::RenderComponent& comp)
    {
        auto meshIt = meshAABBs.find(comp.pMesh);
        return meshIt != meshAABBs.end() ? meshIt->second : comp.boundingBox;
    };

    stltype::hash_map<ECS::EntityID, u32> subMeshCounters;
    for (const auto& renderComp : renderComps)
    {
        u32 subIdx = subMeshCounters[renderComp.entity.ID]++;
        auto& entityMeshes = dataMap[renderComp.entity.ID];
        RenderPasses::EntityMeshData& data = entityMeshes.emplace_back(renderComp.entity.ID,
                                                                       subIdx,
                                                                       renderComp.component.pMesh,
                                                                       renderComp.component.pMaterial,
                                                                       localAABBOf(renderComp.component),
                                                                       false);
        data.SetIncludeInRayTracing(renderComp.component.includeInRayTracing);
        if (renderComp.component.isSelected || renderComp.component.isWireframe)
        {
            data.SetDebugWireframeMesh();
        }
    }
    // Light proxies and other debug shapes, drawn only by DebugShapePass
    for (const auto& debugComp : debugRenderComps)
    {
        if (!debugComp.component.shouldRender)
            continue;
        u32 subIdx = subMeshCounters[debugComp.entity.ID]++;
        dataMap[debugComp.entity.ID].emplace_back(debugComp.entity.ID,
                                                  subIdx,
                                                  debugComp.component.pMesh,
                                                  debugComp.component.pMaterial,
                                                  localAABBOf(debugComp.component),
                                                  true);
    }

    m_pPassManager->SetEntityMeshDataForFrame(std::move(dataMap), currentFrame);
}

bool ECS::System::SRenderComponent::AccessesAnyComponents(const stltype::vector<C_ID>& components)
{
    return (stltype::find(components.begin(), components.end(), ComponentID<Components::RenderComponent>::ID) !=
            components.end()) ||
           (stltype::find(components.begin(), components.end(), ComponentID<Components::DebugRenderComponent>::ID) !=
            components.end());
}
