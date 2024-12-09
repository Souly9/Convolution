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
		m_passManager.BlockUntilPassesFinished(currentFrame);

		// Start imgui frame, update frame numbers
		{
			currentFrame = FrameGlobals::GetFrameNumber();
			lastFrame = FrameGlobals::GetPreviousFrameNumber(currentFrame);

		}
		// First sync game data with renderthread
		{
			g_pEntityManager->SyncSystemData(lastFrame);

		}

		// Sync ended, signal gamethread
		g_renderThreadReadSemaphore.Post();

		{
			m_passManager.PreProcessDataForCurrentFrame();
			g_pQueueHandler->WaitForFences();
		}

		{
			m_passManager.ExecutePasses(FrameGlobals::GetFrameNumber());
		}

		{
			m_pImGuiManager->EndFrame();
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

