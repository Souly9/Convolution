#pragma once
#include "Rendering/RenderLayer.h"

class WindowManager;
class UI;
class TimeData;

class Application
{
public:
	Application(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title);

	void Run();
	void Update();

	void Render();

private:
	WindowManager m_windowManager;

	TimeData m_time{};
	UI m_ui{};

	RenderLayer<RenderAPI> m_renderLayer;
};