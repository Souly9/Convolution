#include "SLight.h"
#include "Core/Rendering/Core/Defines/LightDefines.h"

void ECS::System::SLight::Init(const SystemInitData& data)
{
	m_pPassManager = data.pPassManager;
}

void ECS::System::SLight::Process()
{
}

void ECS::System::SLight::SyncData(u32 currentFrame)
{
	ScopedZone("Light System::SyncData");

	const auto entities = g_pEntityManager->GetEntitiesWithComponent<Components::Light>();
	RenderPasses::PointLightVector lights;
	DirectionalRenderLight dirLight;
	u32 numOfDirLights = 0;
	lights.reserve(entities.size());

	for (const auto& entity : entities)
	{
		const auto* pLightComponent = g_pEntityManager->GetComponentUnsafe<Components::Light>(entity);
		const auto* pTransformComp = g_pEntityManager->GetComponentUnsafe<Components::Transform>(entity);
		if (pLightComponent->type == Components::LightType::Directional)
		{
			if (numOfDirLights >= 1)
			{
				DEBUG_LOG_WARN("More than one directional light found in scene! Only the first one will be used for rendering.");
				continue;
			}
			dirLight = ConvertToDirectionalRenderLight(pLightComponent, pTransformComp);
			numOfDirLights++;
		}
		else
		{

			lights.push_back(ConvertToRenderLight(pLightComponent, pTransformComp));
		}
	}
	m_pPassManager->SetLightDataForFrame(stltype::move(lights), { dirLight }, currentFrame);
}

bool ECS::System::SLight::AccessesAnyComponents(const stltype::vector<C_ID>& components)
{
	return stltype::find(components.begin(), components.end(), ComponentID<Components::Light>::ID) != components.end();
}
