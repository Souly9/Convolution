#pragma once
#include "Controls/SceneGraphWindow.h"
#include "Controls/SelectedEntitiesWindow.h"
#include "DebugWindows/InfoWindow.h"
#include "DebugWindows/MemoryWindow.h"
#include "DebugWindows/PerformanceDiagnosticsWindow.h"
#include "DebugWindows/RenderGraphInspectorWindow.h"
#include "DebugWindows/RenderSettingsWindow.h"
#include "DebugWindows/TextureViewerWindow.h"
#include "ImGuiManager.h"
#include <imgui/imgui.h>

class MainMenuBar
{
public:
    MainMenuBar();

    void DrawMenuBar(f32 dt, ApplicationInfos& appInfos);

    void OnUpdate(const UpdateEventData& d)
    {
        m_lastUpdateState = d;
    }

private:
    void DrawSceneMenu();
    void DrawWindowMenu();
    void DrawStatusText();
    // Matches the default dock layout: editor panels open, big tools closed
    void ResetOpenStates();
    void WindowMenuItem(const char* name, UIWindow& window);

    UpdateEventData m_lastUpdateState;
    // Focused one frame later, once the window exists and can select its dock tab
    const char* m_focusRequest{nullptr};
    LogWindow m_logWindow;
    SelectedEntityWindow m_inspectorWindow;
    SceneGraphWindow m_sceneGraphWindow;
    RenderSettingsWindow m_settingsWindow;
    PerformanceDiagnosticsWindow m_performanceWindow;
    RenderGraphInspectorWindow m_renderGraphWindow;
    TextureViewerWindow m_textureViewerWindow;
    MemoryWindow m_memoryWindow;
};
