#include "Application.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/RenderLayer.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "Scenes/BistroExteriorScene.h"
#include "Scenes/ClusteredLightingScene.h"
#include "Scenes/SampleScene.h"
#include "Scenes/SponzaScene.h"
#include "StaticBehaviors/StaticBehaviorCollection.h"
#include "TimeData.h"
#include <GLFW/glfw3.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
#include "Core/Rendering/Vulkan/VkProfiler.h"
#include "vulkan/vulkan_core.h"


Application::Application(bool canRender, RenderLayer<RenderAPI>& layer) : m_renderThread(&m_imGuiManager)
{
    m_pProfiler = stltype::make_unique<VkProfiler>();
    VkGlobals::SetProfiler(m_pProfiler.get());
    g_pApplicationState = &m_applicationState;
    
    bool bCanRender = layer.InitRenderLayer(
        g_pWindowManager->GetScreenWidth(), g_pWindowManager->GetScreenHeight(), g_pWindowManager->GetTitle());

    g_pGPUMemoryManager->Init();
    m_pProfiler->Init();
    g_pQueueHandler->Init();
    g_pTexManager->Init();
    g_pGlobalTimeData->Reset();

    FrameGlobals::SetFrameNumber(0);
    m_applicationState.SetCurrentScene(stltype::make_unique<SampleScene>());
    g_pShaderManager->ReadAllSourceShaders();

    g_pEventSystem->OnBaseInit({});

    m_applicationState.ProcessStateUpdates();

    auto pRenderer = m_renderThread.Start();
    g_pEventSystem->OnAppInit({pRenderer});
    StaticBehaviorCollection::RegisterAllBehaviors();
    
    m_applicationState.ProcessStateUpdates();
    g_pQueueHandler->WaitForFences();
    Update(0);
    Update(1);
}

void Application::CreateMainPSO()
{
}

Application::~Application()
{
    m_pProfiler->Destroy();
    VkGlobals::SetProfiler(nullptr);
    g_mainRenderThreadSyncSemaphore.Post();
    g_frameTimerSemaphore2.Post();
    g_imguiSemaphore.Post();
    vkDeviceWaitIdle(VkGlobals::GetLogicalDevice());
    m_renderThread.ShutdownThread();

    m_imGuiManager.CleanUp();
}

void Application::Run()
{
    u32 currentFrame = 0;
    u32 lastFrame = 1;
    while (!glfwWindowShouldClose(g_pWindowManager->GetWindow()))
    {
        m_pProfiler->BeginFrame(currentFrame);
        WaitForRendererToFinish();

        {
            lastFrame = currentFrame;
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
    }
    vkDeviceWaitIdle(VkGlobals::GetLogicalDevice());
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
