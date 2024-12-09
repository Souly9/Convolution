#pragma once 
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/EntityManager.h"
#include "../DebugWindows/InfoWindow.h"
#include "ComponentVisualizers/TransformVisualizer.h"
#include "ComponentVisualizers/CameraVisualizer.h"
#include "ComponentVisualizers/LightVisualizer.h"

class SelectedEntityWindow : public InfoWindow
{
public:
	void DrawWindow(const UpdateEventData& data)
	{
		if (data.state.selectedEntities.size() == 0)
			return;
		const auto& selectedEntity = data.state.selectedEntities[0];
		bool isEntityDirty = false;
		ImGui::Begin("Selected Entities", &m_isOpen);
		ECS::Components::Transform* pTransform = g_pEntityManager->GetComponent<ECS::Components::Transform>(selectedEntity);
		isEntityDirty |= Visualize(pTransform);
		ECS::Components::Camera* pCamera = g_pEntityManager->GetComponent<ECS::Components::Camera>(selectedEntity);
		Visualize(pCamera);
		ECS::Components::Light* pLight = g_pEntityManager->GetComponent<ECS::Components::Light>(selectedEntity);
		isEntityDirty |= Visualize(pLight);
		ImGui::End();
		if(isEntityDirty)
		{
			g_pEntityManager->MarkEntityDirty(selectedEntity, ECS::ComponentID<ECS::Components::Transform>::ID);
			g_pEntityManager->MarkEntityDirty(selectedEntity, ECS::ComponentID<ECS::Components::Light>::ID);
		}
	}


private:
};