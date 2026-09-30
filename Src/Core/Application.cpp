#include "Application.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/RenderLayer.h"
#include "Core/Rendering/Backend/BackendGlobals.h"
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
#include <imgui/backends/imgui_impl_glfw.h>
#ifdef USE_VULKAN
#include <imgui/backends/imgui_impl_vulkan.h>
#endif
#include <imgui/imgui.h>

Application::Application(bool canRender, RenderLayer<RenderAPI>& layer)
    : m_renderThread(&m_imGuiManager, &layer.GetBackend())
{
    m_pProfiler = stltype::make_unique<BackendProfiler>();
    RenderGlobals::SetProfiler(m_pProfiler.get());
    g_pApplicationState = &m_applicationState;

    layer.InitRenderLayer(
        g_pWindowManager->GetScreenWidth(), g_pWindowManager->GetScreenHeight(), g_pWindowManager->GetTitle());

    g_pGPUMemoryManager->Init();
    m_pProfiler->Init();
    g_pQueueHandler->Init();
    g_pTexManager->Init();

    // Load placeholder first
    auto placeholderHandle = g_pTexManager->SubmitAsyncTextureCreation(
        {"Resources\\Textures\\placeholder.png", false, TextureSemantic::BaseColor, true});
    g_pTexManager->WaitFor(placeholderHandle);
    g_pTexManager->SetPlaceholder(placeholderHandle);

    g_pGlobalTimeData->Reset();

    FrameGlobals::SetFrameNumber(0);
    // Bistro is a local-only asset; fall back to Sponza when it isn't there
    if (std::filesystem::exists("Resources/Models/BistroExterior.fbx"))
        m_applicationState.SetCurrentScene(stltype::make_unique<BistroExteriorScene>());
    else
        m_applicationState.SetCurrentScene(stltype::make_unique<SponzaScene>());
    g_pShaderManager->ReadAllSourceShaders();
    m_applicationState.ProcessStateUpdates();

    g_pEventSystem->OnBaseInit({});

    m_applicationState.ProcessStateUpdates();

    auto pRenderer = m_renderThread.Start();
    m_applicationState.SetPassManager(pRenderer);
    g_pEventSystem->OnAppInit({pRenderer});
    StaticBehaviorCollection::RegisterAllBehaviors();

    m_applicationState.ProcessStateUpdates();
    g_pQueueHandler->WaitForFences(~0u);
    Update(0);
    Update(1);
}

void Application::CreateMainPSO()
{
}

Application::~Application()
{
    m_renderThread.Stop();

    g_mainRenderThreadSyncSemaphore.Post();
    g_frameTimerSemaphore2.Post();
    g_imguiSemaphore.Post();

    m_renderThread.ShutdownThread();
    SRF::WaitForDeviceIdle<RenderAPI>();

    m_renderThread.CleanUp();

    m_pProfiler->Destroy();
    RenderGlobals::SetProfiler(nullptr);

    g_pDeleteQueue->ForceEmptyQueue();
    m_imGuiManager.CleanUp();
}

void Application::Run()
{
    u32 currentFrame = 0;
    
    while (!glfwWindowShouldClose(g_pWindowManager->GetWindow()))
    {
        WaitForRendererToFinish();

        {
            
            currentFrame = ++currentFrame % FRAMES_IN_FLIGHT;
            FrameGlobals::SetFrameNumber(currentFrame);
        }

        g_frameTimerSemaphore2.Post();

        g_renderThreadReadSemaphore.Wait();
        m_applicationState.ProcessStateUpdates();
        // ImGui accesses the entity manager to update data, which isn't designed
        // for multi-threaded access Hence we run the draw on the main thread and
        // just retrieve the data on the renderthread for simplicity
        m_imGuiManager.BeginFrame();
        m_imGuiManager.RenderElements(0.16f, LogData::Get()->GetApplicationInfos());
        g_imguiSemaphore.Post();

        // Notify all systems the next frame started, mainly used as pre-update
        g_pEventSystem->OnNextFrame({currentFrame});

        // Update game on multiple threads
        Update(currentFrame);

        glfwPollEvents();
        g_pWindowManager->Update();
    }
    SRF::WaitForDeviceIdle<RenderAPI>();
}

void Application::Update(u32 currentFrame)
{
    g_pGlobalTimeData->Step();

    const auto& appState = m_applicationState.GetCurrentApplicationState();
    g_pEventSystem->OnUpdate({appState, g_pGlobalTimeData->GetDeltaTime()});

    g_pEntityManager->UpdateSystems(currentFrame);
}

void Application::WaitForRendererToFinish()
{
    g_mainRenderThreadSyncSemaphore.Post();
    g_frameTimerSemaphore.Wait();
}
