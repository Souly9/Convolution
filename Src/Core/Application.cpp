#include <GLFW/glfw3.h>
#include "Application.h"
#include "Rendering/Passes/StaticMeshPass.h"
#include "Core/ECS/EntityManager.h"
#include "TimeData.h"
#include "Core/Rendering/RenderLayer.h"
#include "Core/Global/GlobalVariables.h"
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>
Application::Application(bool canRender, RenderLayer<RenderAPI>& layer)
{
	g_pApplicationState = &m_applicationState;
	bool bCanRender = layer.InitRenderLayer(g_pWindowManager->GetScreenWidth(), g_pWindowManager->GetScreenHeight(), g_pWindowManager->GetTitle());

	g_pTexManager->Init();
	g_pQueueHandler->Init();

	CreateMainPSO();
	g_pGlobalTimeData->Reset();

	auto ent = g_pEntityManager->CreateEntity();
	ECS::Components::RenderComponent comp{};
	comp.pMesh = g_pMeshManager->GetPrimitiveMesh(MeshManager::PrimitiveType::Quad);
	g_pEntityManager->AddComponent(ent, comp);
	ECS::Components::View compV{};
	g_pEntityManager->AddComponent(ent, compV);
	FrameGlobals::SetFrameNumber(0);
	g_pEntityManager->UpdateSystems();
	g_pEntityManager->SyncSystemData();
	m_passManager.PreProcessDataForCurrentFrame();
	FrameGlobals::SetFrameNumber(1);
	g_pEntityManager->SyncSystemData();
	m_passManager.PreProcessDataForCurrentFrame();
	FrameGlobals::SetFrameNumber(0);
	g_pApplicationState->RegisterUpdateFunction([ent](ApplicationState& state) { state.selectedEntities.push_back(ent); });
}

void Application::CreateMainPSO()
{
	g_pEventSystem->OnBaseInit({});
	g_pEventSystem->OnAppInit({
		&m_passManager
		});
}

Application::~Application()
{
	m_imGuiManager.CleanUp();
	g_pTexManager.reset();
	g_pEntityManager.reset();
	g_pQueueHandler.reset();
	g_pFileReader.reset();
	g_pMeshManager.reset();
	g_pGPUMemoryManager.reset();
}

void Application::Run()
{
	u32 currentFrame = 0;
	while(!glfwWindowShouldClose(g_pWindowManager->GetWindow()))
	{

		Update();
		
		Render();

		glfwPollEvents();

		FrameGlobals::SetFrameNumber(++currentFrame % FRAMES_IN_FLIGHT);
		m_applicationState.ProcessStateUpdates();
	}
	vkDeviceWaitIdle(VkGlobals::GetLogicalDevice());
}

void Application::Update()
{
	g_pGlobalTimeData->Step();

	g_pEntityManager->UpdateSystems();
	const auto& appState = m_applicationState.GetCurrentApplicationState();
	g_pEventSystem->OnUpdate({ appState, g_pGlobalTimeData->GetDeltaTime() });
}

void Application::Render()
{
	m_imGuiManager.BeginFrame();
	m_imGuiManager.RenderElements(0.16f, LogData::Get()->GetApplicationInfos());

	m_passManager.ExecutePasses(FrameGlobals::GetFrameNumber());
	m_imGuiManager.EndFrame();
	g_pTexManager->PostRender();
}
