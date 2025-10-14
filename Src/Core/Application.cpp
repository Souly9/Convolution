#include <GLFW/glfw3.h>
#include "Application.h"
#include "Rendering/Passes/StaticMeshPass.h"
#include "TimeData.h"
#include "Core/Rendering/RenderLayer.h"
#include "Core/Global/GlobalVariables.h"
#include "StaticBehaviors/StaticBehaviorCollection.h"
#include "Scenes/SampleScene.h"
#include "Scenes/SponzaScene.h"
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>

Application::Application(bool canRender, RenderLayer<RenderAPI>& layer) : m_renderThread(&m_imGuiManager)
{
	g_pApplicationState = &m_applicationState;
	bool bCanRender = layer.InitRenderLayer(g_pWindowManager->GetScreenWidth(), g_pWindowManager->GetScreenHeight(), g_pWindowManager->GetTitle());

	g_pGPUMemoryManager->Init();
	g_pQueueHandler->Init();
	g_pTexManager->Init();
	g_pGlobalTimeData->Reset();

	FrameGlobals::SetFrameNumber(0);
	m_applicationState.SetCurrentScene(stltype::make_unique<SponzaScene>());
	g_pShaderManager->ReadAllSourceShaders();

	g_pEventSystem->OnBaseInit({});

	m_applicationState.ProcessStateUpdates();



	auto pRenderer = m_renderThread.Start();
	g_pEventSystem->OnAppInit({
		pRenderer
		});
	StaticBehaviorCollection::RegisterAllBehaviors();
//
//	
//	g_pFileReader->SubmitIORequest(IORequest{ "Resources/Models/bunny.obj", [&](const ReadMeshInfo& info)
//{
//			auto ent = info.rootNode.root;
//			g_pApplicationState->RegisterUpdateFunction([ent](ApplicationState& state) { state.selectedEntities.clear(); state.selectedEntities.push_back(ent); });
//}, RequestType::Mesh });
//	
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
	g_mainRenderThreadSyncSemaphore.Post();
	g_frameTimerSemaphore2.Post();
	g_imguiSemaphore.Post();
	m_renderThread.ShutdownThread();
	vkDeviceWaitIdle(VkGlobals::GetLogicalDevice());

	m_imGuiManager.CleanUp();
}

void Application::Run()
{
	u32 currentFrame = 0;
	u32 lastFrame = 1;
	while(!glfwWindowShouldClose(g_pWindowManager->GetWindow()))
	{
		WaitForRendererToFinish();

		{
			lastFrame = currentFrame;
			currentFrame = ++currentFrame % FRAMES_IN_FLIGHT;
			FrameGlobals::SetFrameNumber(currentFrame);
		}

		g_frameTimerSemaphore2.Post();

		g_renderThreadReadSemaphore.Wait();
		m_applicationState.ProcessStateUpdates();
		// ImGui accesses the entity manager to update data, which isn't designed for multi-threaded access
		// Hence we run the draw on the main thread and just retrieve the data on the renderthread for simplicity
		m_imGuiManager.RenderElements(0.16f, LogData::Get()->GetApplicationInfos());
		g_imguiSemaphore.Post();

		// Notify all systems the next frame started, mainly used as pre-update
		g_pEventSystem->OnNextFrame({ currentFrame });

		// Update game on multiple threads
		Update(currentFrame);

		glfwPollEvents();
	}
}

void Application::Update(u32 currentFrame)
{
	g_pGlobalTimeData->Step();

	const auto& appState = m_applicationState.GetCurrentApplicationState();
	g_pEventSystem->OnUpdate({ appState, g_pGlobalTimeData->GetDeltaTime() });

	g_pEntityManager->UpdateSystems(currentFrame);
}

void Application::WaitForRendererToFinish()
{
	g_mainRenderThreadSyncSemaphore.Post();
	g_frameTimerSemaphore.Wait();
}
