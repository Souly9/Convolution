#pragma once
#include "Core/Events/EventSystem.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/UI/ImGui/ImGuiManager.h"
#include "Core/UI/ImGui/MainMenuBar.h"
#include "RenderThread.h"
#include "Rendering/RenderLayer.h"
#include "TimeData.h"

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
    void Update(u32 currentFrame);

    void Render();

    void WaitForRendererToFinish();

private:
    void CreateMainPSO();

    ImGuiManager m_imGuiManager{};

    RenderThread m_renderThread;

    ApplicationStateManager m_applicationState{};
};