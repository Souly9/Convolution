#pragma once
#include "Controls/SceneGraphWindow.h"
#include "Controls/SelectedEntitiesWindow.h"
#include "DebugWindows/EngineSettingsWindow.h"
#include "DebugWindows/InfoWindow.h"
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
    InfoWindow m_debugInfoWindow;
    SelectedEntityWindow m_selectedEntitiesWindow;
    UpdateEventData m_lastUpdateState;
    SceneGraphWindow m_sceneGraphWindow;
    RenderSettingsWindow m_renderSettingsWindow;
    EngineSettingsWindow m_engineSettingsWindow;
    PerformanceDiagnosticsWindow m_performanceDiagnosticsWindow;
    RenderGraphInspectorWindow m_renderGraphInspectorWindow;
    TextureViewerWindow m_textureViewerWindow;
};
