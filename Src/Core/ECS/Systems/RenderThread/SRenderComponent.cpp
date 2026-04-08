#include "SRenderComponent.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Core/Global/LogDefines.h"

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
    const auto& renderComps = g_pEntityManager->GetComponentVector<Components::RenderComponent>();
    const auto& debugRenderComps = g_pEntityManager->GetComponentVector<Components::DebugRenderComponent>();

    RenderPasses::EntityMeshDataMap dataMap;
    dataMap.reserve(renderComps.size());

    stltype::hash_map<ECS::EntityID, u32> subMeshCounters;
    for (const auto& renderComp : renderComps)
    {
        u32 subIdx = subMeshCounters[renderComp.entity.ID]++;
        RenderPasses::EntityMeshData& data = dataMap[renderComp.entity.ID].emplace_back(
            renderComp.entity.ID, subIdx, renderComp.component.pMesh, renderComp.component.pMaterial, renderComp.component.boundingBox, false);
        if (renderComp.component.isSelected || renderComp.component.isWireframe)
        {
            data.SetDebugWireframeMesh();
        }
    }
    m_pPassManager->SetEntityMeshDataForFrame(std::move(dataMap), currentFrame);
    return;
    // TODO: Add debug render components back in
    for (const auto& renderComp : debugRenderComps)
    {
        if (renderComp.component.shouldRender == false)
            continue;

        u32 subIdx = subMeshCounters[renderComp.entity.ID]++;
        RenderPasses::EntityMeshData& data = dataMap[renderComp.entity.ID].emplace_back(
            renderComp.entity.ID, subIdx, renderComp.component.pMesh, renderComp.component.pMaterial, renderComp.component.boundingBox, true);
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
