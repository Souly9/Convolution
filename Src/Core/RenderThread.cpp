#include "RenderThread.h"

RenderThread::RenderThread(ImGuiManager* pImGuiManager) : m_pImGuiManager(pImGuiManager)
{
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
		m_pImGuiManager->BeginFrame();
		WaitForGameThreadAndPreviousFrame();

		// Start imgui frame, update frame numbers
		{
			lastFrame = currentFrame;
			currentFrame = FrameGlobals::GetFrameNumber();

		}
		// First sync game data with renderthread
		{
			m_passManager.BlockUntilPassesFinished(lastFrame);
			g_pEntityManager->SyncSystemData(lastFrame);

			// Renderer done so we can take care of any deferred deletes
			g_pDeleteQueue->ProcessDeleteQueue();
		}

		// Sync ended, signal gamethread
		g_renderThreadReadSemaphore.Post();
		g_imguiSemaphore.Wait();

		{
			m_passManager.PreProcessDataForCurrentFrame(lastFrame);
			g_pQueueHandler->WaitForFences();
		}

		{
			m_passManager.ExecutePasses(lastFrame);
		}

		{
			g_pTexManager->PostRender();
		}
	}
}

RenderPasses::PassManager* RenderThread::Start()
{
	m_keepRunning = true;
	m_thread = threadSTL::MakeThread([this]() { RenderLoop(); });
	m_thread.SetName("Convolution_RenderThread");
	return &m_passManager;
}

