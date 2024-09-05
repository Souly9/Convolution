#pragma once
#include "Rendering/RenderLayer.h"

class UI;
class TimeData;

class Application
{
public:
	Application(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title);

	~Application();

	void Run();
	void Update();

	void Render();

	void ConsolePrintDebug() const;

private:
	TimeData m_time{};
	UI m_ui{};

	RenderLayer<RenderAPI> m_renderLayer;
};