#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/UI/ImGui/ImGuiManager.h"
#include "Core/UI/ImGui/MainMenuBar.h"
#include "TimeData.h"
#include "Rendering/RenderLayer.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Core/Events/EventSystem.h"

class UI;
class TimeData;
namespace RenderPasses
{
	class PassManager;
}

class Application
{
public:
	Application(bool canRender, RenderLayer<RenderAPI>& m_layer);

	~Application();

	void Run();
	void Update();

	void Render();

private:

	void CreateMainPSO();

	ImGuiManager m_imGuiManager{};
	RenderPasses::PassManager m_passManager;

	ApplicationStateManager m_applicationState{};

};