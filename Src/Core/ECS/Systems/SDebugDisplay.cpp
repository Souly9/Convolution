#include "SDebugDisplay.h"

void ECS::System::SDebugDisplay::Init(const SystemInitData& data)
{
	m_pPassManager = data.pPassManager;

	g_pEventSystem->AddUpdateEventCallback([this](const UpdateEventData& updateData)
		{
			bool prevState = m_renderDebugMeshes;
			m_renderDebugMeshes = updateData.state.ShouldDisplayDebugObjects();
			m_stateChanged = prevState != m_renderDebugMeshes;
		});
}

void ECS::System::SDebugDisplay::Process()
{
	if (!m_stateChanged)
		return;
	bool shouldRender = m_renderDebugMeshes;

	const auto entities = g_pEntityManager->GetAllEntities();

	for (const auto& entity : entities)
	{
		const auto* pLightComponent = g_pEntityManager->GetComponent<Components::Light>(entity);
		if (pLightComponent == nullptr && shouldRender)
		{
			DEBUG_ASSERT(g_pEntityManager->GetComponent<Components::DebugRenderComponent>(entity) == nullptr);
		}
		else if(pLightComponent != nullptr)
		{
			bool hasDebugComp = g_pEntityManager->HasComponent<Components::DebugRenderComponent>(entity);
			if (!shouldRender && !hasDebugComp)
				continue;
			else if(shouldRender && !hasDebugComp)
			{
				Components::DebugRenderComponent lightDebugComp;
				lightDebugComp.pMesh = g_pMeshManager->GetPrimitiveMesh(MeshManager::PrimitiveType::Cube);
				lightDebugComp.pMaterial = &m_debugMaterial;
				g_pEntityManager->AddComponent(entity, lightDebugComp);
				auto pTransform = g_pEntityManager->GetComponentUnsafe<Components::Transform>(entity);
				pTransform->scale = mathstl::Vector3(0.5);
				pTransform->name = "Light";
			}
			else
			{
				auto* pDebugRenderComponent = g_pEntityManager->GetComponentUnsafe<Components::DebugRenderComponent>(entity);
				pDebugRenderComponent->shouldRender = shouldRender;
			}
			g_pEntityManager->MarkComponentDirty({}, ECS::ComponentID<ECS::Components::RenderComponent>::ID);
			g_pEntityManager->MarkComponentDirty({}, ECS::ComponentID<ECS::Components::Transform>::ID);
		}
	}
}

void ECS::System::SDebugDisplay::SyncData(u32 currentFrame)
{
}

bool ECS::System::SDebugDisplay::AccessesAnyComponents(const stltype::vector<C_ID>& components)
{
	return stltype::find(components.begin(), components.end(), ComponentID<Components::DebugRenderComponent>::ID) != components.end() || 
		stltype::find(components.begin(), components.end(), ComponentID<Components::Light>::ID) != components.end();
}
