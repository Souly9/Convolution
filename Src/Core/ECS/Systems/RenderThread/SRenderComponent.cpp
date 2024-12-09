#include "SRenderComponent.h"
#include "Core/Rendering/Passes/PassManager.h"

void ECS::System::SRenderComponent::Init(const SystemInitData& data)
{
	DEBUG_ASSERT(data.pPassManager);
	m_pPassManager = data.pPassManager;
}

void ECS::System::SRenderComponent::Process()
{
}

void ECS::System::SRenderComponent::SyncData()
{
	// Not that beautiful but don't want to get into archetypes for now and the view system won't run often or on many entities either way
	const auto& renderComps = g_pEntityManager->GetComponentVector<Components::RenderComponent>();

	RenderPasses::EntityMeshDataMap dataMap;
	dataMap.reserve(renderComps.size());

	for (const auto& renderComp : renderComps)
	{
		dataMap[renderComp.entity.ID] = RenderPasses::EntityMeshData{ renderComp.component.pMesh, renderComp.component.pMaterial };
	}

	m_pPassManager->SetEntityMeshDataForFrame(std::move(dataMap), FrameGlobals::GetFrameNumber());
}

bool ECS::System::SRenderComponent::AccessesAnyComponents(const stltype::vector<C_ID>& components)
{
	return stltype::find(components.begin(), components.end(), ComponentID<Components::RenderComponent>::ID) != components.end();
}
