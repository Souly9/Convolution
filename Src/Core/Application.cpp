#include <GLFW/glfw3.h>
#include "Application.h"
#include "Rendering/Passes/StaticMeshPass.h"
#include "Core/ECS/EntityManager.h"
#include "TimeData.h"
#include "Core/Rendering/RenderLayer.h"
#include "Core/Global/GlobalVariables.h"
#include "StaticBehaviors/StaticBehaviorCollection.h"
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>

Application::Application(bool canRender, RenderLayer<RenderAPI>& layer) : m_renderThread(&m_imGuiManager)
{
	g_pApplicationState = &m_applicationState;
	bool bCanRender = layer.InitRenderLayer(g_pWindowManager->GetScreenWidth(), g_pWindowManager->GetScreenHeight(), g_pWindowManager->GetTitle());

	g_pTexManager->Init();
	g_pQueueHandler->Init();
	g_pGlobalTimeData->Reset();

	g_pEventSystem->OnBaseInit({});

	auto pRenderer = m_renderThread.Start();

	g_pEventSystem->OnAppInit({
		pRenderer
		});
	StaticBehaviorCollection::RegisterAllBehaviors();

	auto meshEnt = g_pEntityManager->CreateEntity(DirectX::XMFLOAT3(0,0,0));
	ECS::Components::RenderComponent comp{};
	comp.pMesh = g_pMeshManager->GetPrimitiveMesh(MeshManager::PrimitiveType::Quad);
	g_pEntityManager->AddComponent(meshEnt, comp);
	ECS::Components::Light compL{};
	g_pEntityManager->AddComponent(meshEnt, compL);

	auto camEnt = g_pEntityManager->CreateEntity(DirectX::XMFLOAT3(0, 1, 2));
	ECS::Components::Camera compV{};
	g_pEntityManager->AddComponent(camEnt, compV);


	g_pEntityManager->UpdateSystems(0);
	g_pEntityManager->UpdateSystems(1);
	g_pApplicationState->RegisterUpdateFunction([meshEnt, camEnt](ApplicationState& state) { state.selectedEntities.push_back(meshEnt); state.mainCameraEntity = camEnt; });
}

void Application::CreateMainPSO()
{
}

Application::~Application()
{
	m_renderThread.ShutdownThread();
	m_imGuiManager.CleanUp();
}

void Application::Run()
{
	u32 currentFrame = 0;
	u32 lastFrame = 1;

	while(!glfwWindowShouldClose(g_pWindowManager->GetWindow()))
	{
		WaitForRendererToFinish(1);

		{
			m_applicationState.ProcessStateUpdates();
			lastFrame = currentFrame;
			currentFrame = ++currentFrame % FRAMES_IN_FLIGHT;
			FrameGlobals::SetFrameNumber(currentFrame);
		}
		g_frameTimerSemaphore2.Post();

		// ImGui accesses the entity manager to update data, which isn't designed for multi-threaded access
		// Hence we run the draw on the main thread and just retrieve the data on the renderthread for simplicity
		m_imGuiManager.RenderElements(0.16f, LogData::Get()->GetApplicationInfos());

		g_renderThreadReadSemaphore.Wait();

		g_pEventSystem->OnNextFrame({ currentFrame });

		// Update game on multiple threads
		Update(currentFrame);

		glfwPollEvents();
	}
	vkDeviceWaitIdle(VkGlobals::GetLogicalDevice());
}

void Application::Update(u32 currentFrame)
{
	g_pGlobalTimeData->Step();

	g_pEntityManager->UpdateSystems(currentFrame);
	const auto& appState = m_applicationState.GetCurrentApplicationState();
	g_pEventSystem->OnUpdate({ appState, g_pGlobalTimeData->GetDeltaTime() });


}

void Application::WaitForRendererToFinish(u32 idx)
{
	g_mainRenderThreadSyncSemaphore.Post();
	g_frameTimerSemaphore.Wait();
}
