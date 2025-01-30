#pragma once
#include <imgui/imgui.h>
#include "UIElement.h"
#include "DebugWindows/InfoWindow.h"
#include "DebugWindows/StatsWindow.h"
#include "DebugWindows/DebugSettings.h"
#include "Controls/SelectedEntitiesWindow.h"

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
};
