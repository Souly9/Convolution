#pragma once 
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/EntityManager.h"
#include "../DebugWindows/InfoWindow.h"
#include "ComponentVisualizers/TransformVisualizer.h"

class SelectedEntityWindow : public InfoWindow
{
public:
	void DrawWindow(const UpdateEventData& data)
	{
		if (data.state.selectedEntities.size() == 0)
			return;
		ImGui::Begin("Selected Entities", &m_isOpen);
		const ECS::Components::Transform* pTransform = g_pEntityManager->GetComponent<ECS::Components::Transform>(data.state.selectedEntities[0]);
		Visualize(pTransform);
		ImGui::End();
	}


private:
};