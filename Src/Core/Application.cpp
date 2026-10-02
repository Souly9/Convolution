#include "Application.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/Utils/DeleteQueue.h"
#include "Scenes/BistroExteriorScene.h"
#include "Scenes/ClusteredLightingScene.h"
#include "Scenes/SampleScene.h"
#include "Scenes/SponzaScene.h"
#include "StaticBehaviors/StaticBehaviorCollection.h"
#include "TimeData.h"
#include "Core/Rendering/Core/StaticFunctions.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <imgui/imgui.h>

Application::Application() : m_renderThread(&m_imGuiManager)
{
    // Load placeholder first
    auto placeholderHandle = g_renderer.GetTextureManager().SubmitAsyncTextureCreation(
        {"Resources\\Textures\\placeholder.png", false, TextureSemantic::BaseColor, true});
    g_renderer.GetTextureManager().WaitFor(placeholderHandle);
    g_renderer.GetTextureManager().SetPlaceholder(placeholderHandle);

    g_engine.GetTime().Reset();

    g_engine.SetFrameNumber(0);
    // Bistro is a local-only asset; fall back to Sponza when it isn't there
    if (std::filesystem::exists("Resources/Models/BistroExterior.fbx"))
        g_engine.GetApplicationState().SetCurrentScene(stltype::make_unique<BistroExteriorScene>());
    else
        g_engine.GetApplicationState().SetCurrentScene(stltype::make_unique<SponzaScene>());
    g_renderer.GetShaderManager().ReadAllSourceShaders();
    g_engine.GetApplicationState().ProcessStateUpdates();

    g_engine.GetEventSystem().OnBaseInit({});

    g_engine.GetApplicationState().ProcessStateUpdates();

    auto pRenderer = m_renderThread.Start();
    g_engine.GetEventSystem().OnAppInit({pRenderer});
    StaticBehaviorCollection::RegisterAllBehaviors();

    g_engine.GetApplicationState().ProcessStateUpdates();
    // Init-time uploads would otherwise sit unsubmitted until the first frame reuses the slot
    g_renderer.GetQueueHandler().SubmitUploads(g_renderer.GetRecordingFrameIndex());
    g_renderer.GetQueueHandler().WaitForFences(~0u);
    Update(0);
    Update(1);
}

void Application::CreateMainPSO()
{
}

Application::~Application()
{
    m_renderThread.Stop();

    g_engine.GetFrameSync().mainRenderThreadSync.Post();
    g_engine.GetFrameSync().frameTimer2.Post();
    g_engine.GetFrameSync().imgui.Post();

    m_renderThread.ShutdownThread();
    SRF::WaitForDeviceIdle<RenderAPI>();

    // Before the pass manager goes away: ImGuiPass owns the descriptor pool the ImGui backend frees into
    m_imGuiManager.CleanUp();
    m_renderThread.CleanUp();

    g_renderer.ReleaseProfiler();

    g_renderer.GetDeleteQueue().ForceEmptyQueue();
}

void Application::Run()
{
    u32 currentFrame = 0;
    
    while (!glfwWindowShouldClose(g_engine.GetWindowManager().GetWindow()))
    {
        WaitForRendererToFinish();

        {
            
            currentFrame = ++currentFrame % FRAMES_IN_FLIGHT;
            g_engine.SetFrameNumber(currentFrame);
        }

        g_engine.GetFrameSync().frameTimer2.Post();

        g_engine.GetFrameSync().renderThreadRead.Wait();
        g_engine.GetApplicationState().ProcessStateUpdates();
        // ImGui accesses the entity manager to update data, which isn't designed
        // for multi-threaded access Hence we run the draw on the main thread and
        // just retrieve the data on the renderthread for simplicity
        m_imGuiManager.BeginFrame();
        ApplicationInfos newLogs;
        LogData::Get()->TakeApplicationInfos(newLogs);
        m_imGuiManager.RenderElements(0.16f, newLogs);
        g_engine.GetFrameSync().imgui.Post();

        // Notify all systems the next frame started, mainly used as pre-update
        g_engine.GetEventSystem().OnNextFrame({currentFrame});

        // Update game on multiple threads
        Update(currentFrame);

        glfwPollEvents();
        g_engine.GetWindowManager().Update();
    }
    SRF::WaitForDeviceIdle<RenderAPI>();
}

void Application::Update(u32 currentFrame)
{
    g_engine.GetTime().Step();

    const auto& appState = g_engine.GetApplicationState().GetCurrentApplicationState();
    g_engine.GetEventSystem().OnUpdate({appState, g_engine.GetTime().GetDeltaTime()});

    g_engine.GetEntityManager().UpdateSystems(currentFrame);
}

void Application::WaitForRendererToFinish()
{
    g_engine.GetFrameSync().mainRenderThreadSync.Post();
    g_engine.GetFrameSync().frameTimer.Wait();
}
