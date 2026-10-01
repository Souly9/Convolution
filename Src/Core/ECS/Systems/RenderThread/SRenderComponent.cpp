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

    stltype::hash_map<ECS::EntityID, u32> subMeshCounters;
    for (const auto& renderComp : renderComps)
    {
        u32 subIdx = subMeshCounters[renderComp.entity.ID]++;
        AABB localAABB = renderComp.component.boundingBox;
        if (renderComp.component.pMesh)
        {
            auto meshIt = meshAABBs.find(renderComp.component.pMesh);
            if (meshIt != meshAABBs.end())
            {
                localAABB = meshIt->second;
            }
        }
        RenderPasses::EntityMeshData& data = dataMap[renderComp.entity.ID].emplace_back(
            renderComp.entity.ID, subIdx, renderComp.component.pMesh, renderComp.component.pMaterial, localAABB, false);
        data.SetIncludeInRayTracing(renderComp.component.includeInRayTracing);
        if (renderComp.component.isSelected || renderComp.component.isWireframe)
        {
            data.SetDebugWireframeMesh();
        }
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
