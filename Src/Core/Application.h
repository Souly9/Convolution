#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/UI/UIManager.h"
#include "TimeData.h"
#include "Rendering/RenderLayer.h"

class UI;
class TimeData;
class StaticMainMeshPass;

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

	void CreateMainPSO();

	TimeData m_time{};
	UI m_ui{};

	RenderLayer<RenderAPI> m_renderLayer;

	stltype::unique_ptr<StaticMainMeshPass> m_staticMeshPass;
};