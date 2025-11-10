#include <imgui/imgui.h>
#include "MainMenuBar.h"
#include "Scenes/SampleScene.h"
#include "Scenes/SponzaScene.h"

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
			if (ImGui::MenuItem("Scene Hierarchy", ""))
			{
				m_sceneGraphWindow.SetOpen(true);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Scene"))
		{
			if (ImGui::MenuItem("Reload Current Scene", ""))
			{
				if (g_pApplicationState->GetCurrentScene() != nullptr)
				{
					g_pApplicationState->ReloadCurrentScene();
				}
			}

			if (ImGui::BeginMenu("Load Scene", ""))
			{
				stltype::vector<stltype::string> sceneNames = { SampleScene::GetSceneName(), SponzaScene::GetSceneName() };
				const auto& currentSceneName = g_pApplicationState->GetCurrentScene()->GetName();
				for (auto& name : sceneNames)
				{
					if (ImGui::MenuItem(name.c_str(), "", false, name != currentSceneName))
					{
						if (name == SampleScene::GetSceneName())
						{
							g_pApplicationState->SetCurrentScene(stltype::make_unique<SampleScene>());
						}
						else if (name == SponzaScene::GetSceneName())
						{
							g_pApplicationState->SetCurrentScene(stltype::make_unique<SponzaScene>());
						}
						else
						{
							DEBUG_ASSERT(false);
						}
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Debug"))
		{
			if (ImGui::MenuItem("Log", ""))
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
			if (ImGui::MenuItem("GBuffer", ""))
			{
				m_gbufferWindow.SetOpen(true);
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
	if (m_sceneGraphWindow.IsOpen())
	{
		m_sceneGraphWindow.DrawWindow(m_lastUpdateState);
	}

	if (m_debugSettingsWindow.IsOpen())
	{
		m_debugSettingsWindow.DrawWindow(dt);
	}

	if (m_gbufferWindow.IsOpen())
	{
		m_gbufferWindow.DrawWindow(dt);
	}
}
