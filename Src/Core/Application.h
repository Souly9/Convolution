#pragma once
#include "Core/Events/EventSystem.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/UI/ImGui/ImGuiManager.h"
#include "Core/UI/ImGui/MainMenuBar.h"
#include "RenderThread.h"
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
    Application();

    ~Application();

    void Run();
    void Update(u32 currentFrame);

    void Render();

    void WaitForRendererToFinish();

    // False when the render device couldn't be created; Run() must not be called then
    bool IsInitialized() const
    {
        return m_initialized;
    }

private:
    void CreateMainPSO();

    ImGuiManager m_imGuiManager{};

    RenderThread m_renderThread;

    stltype::unique_ptr<MainMenuBar> m_pMainMenuBar;

    bool m_initialized{false};
};