#pragma once
#include "Controls/SceneGraphWindow.h"
#include "Controls/SelectedEntitiesWindow.h"
#include "DebugWindows/DebugSettings.h"
#include "DebugWindows/GBufferWindow.h"
#include "DebugWindows/InfoWindow.h"
#include "DebugWindows/RenderSettingsWindow.h"
#include "DebugWindows/StatsWindow.h"
#include "UIElement.h"
#include <imgui/imgui.h>

class MainMenuBar : public SelfInstantiatingUIElement<MainMenuBar>
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
    StatsWindow m_statsWindow;
    DebugSettingsWindow m_debugSettingsWindow;
    SelectedEntityWindow m_selectedEntitiesWindow;
    UpdateEventData m_lastUpdateState;
    GBufferWindow m_gbufferWindow;
    SceneGraphWindow m_sceneGraphWindow;
    RenderSettingsWindow m_renderSettingsWindow;
};
