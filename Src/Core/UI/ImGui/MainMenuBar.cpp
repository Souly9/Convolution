#include "MainMenuBar.h"
#include "Scenes/BistroExteriorScene.h"
#include "Scenes/ClusteredLightingScene.h"
#include "Scenes/SampleScene.h"
#include "Scenes/SponzaScene.h"
#include <imgui/imgui.h>
#include "Core/Global/Profiling.h"

namespace
{
struct SceneEntry
{
    stltype::string name;
    stltype::unique_ptr<Scene> (*create)();
};

template <typename T>
stltype::unique_ptr<Scene> CreateScene()
{
    return stltype::make_unique<T>();
}
} // namespace

MainMenuBar::MainMenuBar()
{
    ImGuiManager::RegisterRenderFunction([this](f32 dt, ApplicationInfos& appInfos) { DrawMenuBar(dt, appInfos); });
    g_engine.GetEventSystem().AddUpdateEventCallback([this](const UpdateEventData& d) { OnUpdate(d); });
    ResetOpenStates();
}

void MainMenuBar::ResetOpenStates()
{
    m_sceneGraphWindow.SetOpen(true);
    m_inspectorWindow.SetOpen(true);
    m_settingsWindow.SetOpen(true);
    m_logWindow.SetOpen(true);
    m_performanceWindow.SetOpen(true);
    m_renderGraphWindow.SetOpen(false);
    m_textureViewerWindow.SetOpen(false);
    m_memoryWindow.SetOpen(false);
}

void MainMenuBar::DrawMenuBar(f32 dt, ApplicationInfos& appInfos)
{
    ScopedZone("MainMenuBar");
    m_logWindow.Consume(appInfos);
    if (m_focusRequest != nullptr)
    {
        ImGui::SetWindowFocus(m_focusRequest);
        m_focusRequest = nullptr;
    }

    if (ImGui::BeginMainMenuBar())
    {
        DrawSceneMenu();
        DrawWindowMenu();
        DrawStatusText();
        ImGui::EndMainMenuBar();
    }

    m_inspectorWindow.DrawGizmo(m_lastUpdateState);

    if (m_sceneGraphWindow.IsOpen())
        m_sceneGraphWindow.DrawWindow(m_lastUpdateState);
    if (m_inspectorWindow.IsOpen())
        m_inspectorWindow.DrawWindow(m_lastUpdateState);
    if (m_settingsWindow.IsOpen())
        m_settingsWindow.DrawWindow(dt);
    if (m_logWindow.IsOpen())
        m_logWindow.DrawWindow();
    if (m_performanceWindow.IsOpen())
        m_performanceWindow.DrawWindow(dt);
    if (m_renderGraphWindow.IsOpen())
        m_renderGraphWindow.DrawWindow(dt);
    // Always called: it opens itself when a material texture asks for it
    m_textureViewerWindow.DrawWindow(dt);
    if (m_memoryWindow.IsOpen())
        m_memoryWindow.DrawWindow(dt);
}

void MainMenuBar::DrawSceneMenu()
{
    if (!ImGui::BeginMenu("Scene"))
        return;

    const SceneEntry scenes[] = {{SampleScene::GetSceneName(), &CreateScene<SampleScene>},
                                 {SponzaScene::GetSceneName(), &CreateScene<SponzaScene>},
                                 {BistroExteriorScene::GetSceneName(), &CreateScene<BistroExteriorScene>},
                                 {ClusteredLightingScene::GetSceneName(), &CreateScene<ClusteredLightingScene>}};

    auto& appState = g_engine.GetApplicationState();
    const Scene* pCurrentScene = appState.GetCurrentScene();
    for (const SceneEntry& scene : scenes)
    {
        const bool isCurrent = pCurrentScene != nullptr && pCurrentScene->GetName() == scene.name;
        if (ImGui::MenuItem(scene.name.c_str(), nullptr, isCurrent, !isCurrent))
            appState.SetCurrentScene(scene.create());
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Reload Current Scene", nullptr, false, pCurrentScene != nullptr))
        appState.ReloadCurrentScene();
    ImGui::EndMenu();
}

void MainMenuBar::DrawWindowMenu()
{
    if (!ImGui::BeginMenu("Window"))
        return;

    WindowMenuItem(UIWindowNames::Scene, m_sceneGraphWindow);
    WindowMenuItem(UIWindowNames::Inspector, m_inspectorWindow);
    WindowMenuItem(UIWindowNames::Settings, m_settingsWindow);
    WindowMenuItem(UIWindowNames::Log, m_logWindow);
    WindowMenuItem(UIWindowNames::Performance, m_performanceWindow);
    ImGui::SeparatorText("Tools");
    WindowMenuItem(UIWindowNames::RenderGraph, m_renderGraphWindow);
    WindowMenuItem(UIWindowNames::TextureViewer, m_textureViewerWindow);
    WindowMenuItem(UIWindowNames::Memory, m_memoryWindow);
    ImGui::Separator();
    if (ImGui::MenuItem("Reset Layout"))
    {
        ResetOpenStates();
        ImGuiManager::RequestLayoutReset();
    }
    ImGui::EndMenu();
}

void MainMenuBar::WindowMenuItem(const char* name, UIWindow& window)
{
    if (ImGui::MenuItem(name, nullptr, window.OpenFlag()) && window.IsOpen())
        m_focusRequest = name;
}

// Right-aligned: error badge (opens the log) and frame timings
void MainMenuBar::DrawStatusText()
{
    char status[96];
    snprintf(status,
             sizeof(status),
             "%u FPS  %.2f ms  GPU %.2f ms",
             m_performanceWindow.GetFPS(),
             m_performanceWindow.GetFrameMs(),
             m_performanceWindow.GetGPUMs());

    char errors[32] = {};
    const u32 errorCount = m_logWindow.GetUnseenErrorCount();
    if (errorCount > 0)
        snprintf(errors, sizeof(errors), "%u error%s", errorCount, errorCount == 1 ? "" : "s");

    const ImGuiStyle& style = ImGui::GetStyle();
    f32 width = ImGui::CalcTextSize(status).x + style.ItemSpacing.x;
    if (errorCount > 0)
        width += ImGui::CalcTextSize(errors).x + style.FramePadding.x * 2.0f + style.ItemSpacing.x;
    ImGui::SameLine(ImGui::GetWindowWidth() - width - style.WindowPadding.x);

    if (errorCount > 0)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        if (ImGui::SmallButton(errors))
        {
            m_logWindow.SetOpen(true);
            m_focusRequest = UIWindowNames::Log;
        }
        ImGui::PopStyleColor();
    }
    ImGui::TextDisabled("%s", status);
}
