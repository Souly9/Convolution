#include "SLight.h"

void ECS::System::SLight::Init(const SystemInitData& data)
{
	m_pPassManager = data.pPassManager;
}

void ECS::System::SLight::Process()
{
}

void ECS::System::SLight::SyncData()
{
	const auto entities = g_pEntityManager->GetEntitiesWithComponent<Components::Light>();
	RenderPasses::LightVector lights;
	lights.reserve(entities.size());

	for (const auto& entity : entities)
	{
		const auto* pLightComponent = g_pEntityManager->GetComponentUnsafe<Components::Light>(entity);
		const auto* pTransformComp = g_pEntityManager->GetComponentUnsafe<Components::Transform>(entity);

		lights.push_back(ConvertToRenderLight(pLightComponent, pTransformComp));
	}
	m_pPassManager->SetLightDataForFrame(lights, FrameGlobals::GetFrameNumber());
}

bool ECS::System::SLight::AccessesAnyComponents(const stltype::vector<C_ID>& components)
{
	return stltype::find(components.begin(), components.end(), ComponentID<Components::Light>::ID) != components.end();
}
