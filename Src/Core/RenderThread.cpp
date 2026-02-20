#include "RenderThread.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Core/Utils/DeleteQueue.h"

RenderThread::RenderThread(ImGuiManager* pImGuiManager) : m_pImGuiManager(pImGuiManager)
{
    m_passManager = stltype::make_unique<RenderPasses::PassManager>();
    m_keepRunning = false;
}

void RenderThread::WaitForGameThreadAndPreviousFrame()
{
    g_mainRenderThreadSyncSemaphore.Wait();
    g_frameTimerSemaphore.Post();
    g_frameTimerSemaphore2.Wait();
}

void RenderThread::RenderLoop()
{
    auto currentFrame = FrameGlobals::GetFrameNumber();
    auto lastFrame = FrameGlobals::GetPreviousFrameNumber(currentFrame);

    while (KeepRunning())
    {
        WaitForGameThreadAndPreviousFrame();

        if (!KeepRunning())
            break;

        // Start imgui frame, update frame numbers
        {
            lastFrame = currentFrame;
            currentFrame = FrameGlobals::GetFrameNumber();
        }
        // First sync game data with renderthread
        {
            g_pEntityManager->SyncSystemData(lastFrame);

            m_passManager->BlockUntilPassesFinished(lastFrame);
            // Renderer done so we can take care of any deferred deletes
            g_pDeleteQueue->ProcessDeleteQueue();
        }

        // Sync ended, signal gamethread
        g_renderThreadReadSemaphore.Post();
        g_imguiSemaphore.Wait();
        
        if (!KeepRunning())
            break;

        {
            m_passManager->PreProcessDataForCurrentFrame(lastFrame);
        }

        {
            g_pQueueHandler->WaitForFences();
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
}
