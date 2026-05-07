#include "RenderThread.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Global/FrameGlobals.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/StaticFunctions.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Core/Utils/DeleteQueue.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "Core/Rendering/Vulkan/VkProfiler.h"

RenderThread::RenderThread(ImGuiManager* pImGuiManager, RenderBackendImpl<RenderAPI>* pRenderBackend)
    : m_pImGuiManager(pImGuiManager), m_pRenderBackend(pRenderBackend)
{
    m_passManager = stltype::make_unique<RenderPasses::PassManager>();
    m_keepRunning = false;
    g_pEventSystem->AddSwapchainRecreationEventCallback(
        [this](const SwapchainRecreationEventData&)
        {
            m_swapchainRecreationRequested.store(true, std::memory_order_release);
        });
}

void RenderThread::WaitForGameThreadAndPreviousFrame()
{
    g_mainRenderThreadSyncSemaphore.Wait();
    g_frameTimerSemaphore.Post();
    g_frameTimerSemaphore2.Wait();
}

bool RenderThread::HandleResizeAtFrameStart()
{
    const bool swapchainResizeRequested = m_swapchainRecreationRequested.exchange(false, std::memory_order_acq_rel);
    const bool renderTargetsResizeRequested =
        m_passManager->NeedsResizeDependentResourceRecreate(FrameGlobals::GetSwapChainExtent());
    if (!swapchainResizeRequested && !renderTargetsResizeRequested)
        return true;

    g_pQueueHandler->DispatchAllRequests();
    g_pQueueHandler->WaitForFences(~0u);
    SRF::WaitForDeviceIdle<RenderAPI>();

    bool swapchainRecreated = false;
    if (swapchainResizeRequested && m_pRenderBackend != nullptr)
    {
        swapchainRecreated = m_pRenderBackend->RecreateSwapChain();
        if (!swapchainRecreated)
        {
            m_swapchainRecreationRequested.store(true, std::memory_order_release);
            return false;
        }
    }

    m_passManager->RecreateResizeDependentResources(FrameGlobals::GetSwapChainExtent(), swapchainRecreated);
    return true;
}

void RenderThread::RenderLoop()
{
    auto currentFrame = FrameGlobals::GetFrameNumber();
    auto lastFrame = FrameGlobals::GetPreviousFrameNumber(currentFrame);
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
            currentFrame = FrameGlobals::GetFrameNumber();
            currentJitterFrameNumber = jitterFrameNumber++;
        }
        // First sync game data with renderthread

        g_pEntityManager->SyncSystemData(lastFrame);

        if (!HandleResizeAtFrameStart())
        {
            g_renderThreadReadSemaphore.Post();
            g_imguiSemaphore.Wait();
            continue;
        }

        const bool acquiredFrame = m_passManager->BlockUntilPassesFinished(lastFrame);
        // All previous frame's command buffers have finished executing, safe to process deferred deletes
        g_pDeleteQueue->ProcessDeleteQueue();

        // Sync ended, signal gamethread
        g_renderThreadReadSemaphore.Post();
        g_imguiSemaphore.Wait();
        if (!KeepRunning())
        {
            break;
        }

        if (!acquiredFrame)
        {
            continue;
        }
        {
            m_passManager->PreProcessDataForCurrentFrame(lastFrame, currentJitterFrameNumber);
        }

        {
            VkGlobals::GetProfiler()->PublishResults(lastFrame);
            VkGlobals::GetProfiler()->ResetFrame(lastFrame);
            m_passManager->ReadAndPublishTimingResults(lastFrame);
            m_passManager->ExecutePasses(lastFrame);
        }

        {
            g_pEventSystem->OnPostFrame({lastFrame});
            g_pTexManager->PostRender();
        }
    }
}

RenderPasses::PassManager* RenderThread::Start()
{
    m_thread = threadstl::MakeThread([this]() { RenderLoop(); });
    InitializeThread("Convolution_RenderThread");
    m_keepRunning = true;
    return m_passManager.get();
}

void RenderThread::CleanUp()
{
    m_passManager.reset();
    g_pDeleteQueue->ForceEmptyQueue();
}
