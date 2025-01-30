#include <imgui/imgui.h>
#include "MainMenuBar.h"

MainMenuBar::MainMenuBar() : SelfInstantiatingUIElement()
{
	ImGuiManager::RegisterRenderFunction([this](f32 dt, ApplicationInfos& appInfos) { DrawMenuBar(dt, appInfos); });
	m_debugInfoWindow.SetOpen(false);
	m_debugInfoWindow.Clear();
	{
		g_pEventSystem->AddUpdateEventCallback([this](const UpdateEventData& d) { OnUpdate(d); });
	}
}

void MainMenuBar::DrawMenuBar(f32 dt, ApplicationInfos& appInfos)
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Entity"))
		{
			if (ImGui::MenuItem("Selected Entity Editor", ""))
			{
				m_selectedEntitiesWindow.SetOpen(true);
			}
			ImGui::EndMenu();
		}

		if(ImGui::BeginMenu("Debug"))
		{
			if(ImGui::MenuItem("Log", ""))
			{
				m_debugInfoWindow.SetOpen(true);
			}
			if (ImGui::MenuItem("Stats", ""))
			{
				m_statsWindow.SetOpen(true);
			}
			if (ImGui::MenuItem("Debug Settings", ""))
			{
				m_debugSettingsWindow.SetOpen(true);
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	if (m_debugInfoWindow.IsOpen())
	{
		m_debugInfoWindow.DrawWindow(dt, appInfos);
		appInfos.infos.clear();
	}
	if (m_statsWindow.IsOpen())
	{
		m_statsWindow.DrawWindow(dt);
	}
	if (m_selectedEntitiesWindow.IsOpen())
	{
		m_selectedEntitiesWindow.DrawWindow(m_lastUpdateState);
	}

	if (m_debugSettingsWindow.IsOpen())
	{
		m_debugSettingsWindow.DrawWindow(dt);
	}
}
