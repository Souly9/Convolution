#pragma once 
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/EntityManager.h"
#include "../DebugWindows/InfoWindow.h"

class SelectedEntityWindow : public InfoWindow
{
public:
	void DrawWindow(const UpdateEventData& data)
	{
		ImGui::Begin("Scene", &m_isOpen);

		ImGui::End();

	}


private:
};