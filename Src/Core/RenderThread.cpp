#include "RenderThread.h"
#include "Core/Rendering/Core/Profiler.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Rendering/Core/StaticFunctions.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Core/Utils/DeleteQueue.h"
#include "Core/SceneGraph/SceneStreamer.h"

RenderThread::RenderThread(ImGuiManager* pImGuiManager) : m_pImGuiManager(pImGuiManager)
{
    m_passManager = stltype::make_unique<RenderPasses::PassManager>();
    m_keepRunning = false;
    g_engine.GetEventSystem().AddSwapchainRecreationEventCallback(
        [this](const SwapchainRecreationEventData&)
        {
            m_swapchainRecreationRequested.store(true, std::memory_order_release);
        });
}

void RenderThread::WaitForGameThreadAndPreviousFrame()
{
    g_engine.GetFrameSync().mainRenderThreadSync.Wait();
    g_engine.GetFrameSync().frameTimer.Post();
    g_engine.GetFrameSync().frameTimer2.Wait();
}

bool RenderThread::HandleResizeAtFrameStart()
{
    ScopedZone("Handle Resize");
    const bool swapchainResizeRequested = m_swapchainRecreationRequested.exchange(false, std::memory_order_acq_rel);
    const bool renderTargetsResizeRequested =
        m_passManager->NeedsResizeDependentResourceRecreate(g_renderer.GetSwapchainExtent());
    if (!swapchainResizeRequested && !renderTargetsResizeRequested)
        return true;

    g_renderer.GetQueueHandler().DispatchAllRequests();
    g_renderer.GetQueueHandler().WaitForFences(~0u);
    SRF::WaitForDeviceIdle<RenderAPI>();

    if (swapchainResizeRequested && !g_renderer.RecreateSwapchain())
    {
        m_swapchainRecreationRequested.store(true, std::memory_order_release);
        return false;
    }

    m_passManager->RecreateResizeDependentResources(g_renderer.GetSwapchainExtent());
    return true;
}

void RenderThread::HandleSceneSwitchAtFrameStart()
{
    ScopedZone("Handle Scene Switch");
    if (!g_engine.GetApplicationState().HasPendingSceneSwitch())
        return;

    // The switch left the device idle, the old scene's entities and meshes are gone
    if (g_engine.GetApplicationState().ExecuteSceneSwitchOnRenderThread())
        m_passManager->ResetSceneState();
}

void RenderThread::RenderLoop()
{
    auto currentFrame = g_engine.GetFrameNumber();
    auto lastFrame = Engine::GetPreviousFrameNumber(currentFrame);
    u64 jitterFrameNumber = 0;

    while (KeepRunning())
    {
        WaitForGameThreadAndPreviousFrame();
        if (!KeepRunning())
        {
            break;
        }

        // Start imgui frame, update frame numbers
        u64 currentJitterFrameNumber = 0;
        {
            lastFrame = currentFrame;
            currentFrame = g_engine.GetFrameNumber();
            currentJitterFrameNumber = jitterFrameNumber++;
            g_renderer.SetRecordingFrameIndex(lastFrame);
        }
        // First sync game data with renderthread

        g_engine.GetEntityManager().SyncSystemData(lastFrame);

        if (!HandleResizeAtFrameStart())
        {
            g_engine.GetFrameSync().renderThreadRead.Post();
            g_engine.GetFrameSync().imgui.Wait();
            continue;
        }

        HandleSceneSwitchAtFrameStart();

        const bool acquiredFrame = m_passManager->BlockUntilPassesFinished(lastFrame);
        // All previous frame's command buffers have finished executing, safe to process deferred deletes
        g_renderer.GetDeleteQueue().ProcessDeleteQueue();
        // The game thread is parked until the Post below, so load callbacks and streaming may touch the ECS and the GPU
        const auto& streaming = g_engine.GetApplicationState().GetCurrentApplicationState().engineState.streaming;
        g_engine.GetFileReader().DeliverCompleted(streaming.texturesPerFrame);
        g_engine.GetSceneStreamer().Tick();

        // Sync ended, signal gamethread
        g_engine.GetFrameSync().renderThreadRead.Post();
        g_engine.GetFrameSync().imgui.Wait();
        if (!KeepRunning())
        {
            break;
        }

        if (!acquiredFrame)
        {
            // Uploads recorded this iteration must not sit around until the slot comes back
            g_renderer.GetQueueHandler().SubmitUploads(lastFrame);
            continue;
        }
        {
            m_passManager->PreProcessDataForCurrentFrame(lastFrame, currentJitterFrameNumber);
        }

        {
            g_renderer.TryGetProfiler()->PublishResults(lastFrame);
            g_renderer.TryGetProfiler()->ResetFrame(lastFrame);
            m_passManager->ReadAndPublishTimingResults(lastFrame);
            m_passManager->ExecutePasses(lastFrame);
        }

        {
            g_engine.GetEventSystem().OnPostFrame({lastFrame});
        }
    }
}

RenderPasses::PassManager* RenderThread::Start()
{
    m_keepRunning = true;
    g_engine.GetApplicationState().SetRenderThreadRunning(true);
    m_thread = threadstl::MakeThread([this]() { RenderLoop(); });
    InitializeThread("Convolution_RenderThread");
    return m_passManager.get();
}

void RenderThread::CleanUp()
{
    g_engine.GetApplicationState().SetRenderThreadRunning(false);
    m_passManager.reset();
    g_renderer.GetDeleteQueue().ForceEmptyQueue();
}
