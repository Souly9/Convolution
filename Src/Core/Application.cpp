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

	FrameGlobals::SetFrameNumber(0);

	auto pRenderer = m_renderThread.Start();

	g_pEventSystem->OnAppInit({
		pRenderer
		});
	StaticBehaviorCollection::RegisterAllBehaviors();

	auto meshEnt = g_pEntityManager->CreateEntity(DirectX::XMFLOAT3(0,0,0));
	ECS::Components::RenderComponent comp{};
	g_pFileReader->SubmitIORequest(IORequest{ "Resources/Models/bunny.obj", [&](const ReadMeshInfo& info)
{
			g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(info.rootNode.root)->position = DirectX::XMFLOAT3(3, 2, 0);
}, RequestType::Mesh });
	comp.pMesh = g_pMeshManager->GetPrimitiveMesh(MeshManager::PrimitiveType::Cube);
	comp.pMaterial = g_pMaterialManager->AllocateMaterial("DefaultConvolutionMaterial", Material{});
	comp.pMaterial->properties.baseColor = mathstl::Vector4{ 1,1,1,1 };
	g_pEntityManager->AddComponent(meshEnt, comp);

	auto parentEnt = g_pEntityManager->CreateEntity(DirectX::XMFLOAT3(0, 1, 0));
	comp.pMesh = g_pMeshManager->GetPrimitiveMesh(MeshManager::PrimitiveType::Cube);
	g_pEntityManager->AddComponent(parentEnt, comp);
	//g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(meshEnt)->parent = parentEnt;

	auto camEnt = g_pEntityManager->CreateEntity(DirectX::XMFLOAT3(4, 0, 12));
	auto* pTransform = g_pEntityManager->GetComponentUnsafe<ECS::Components::Transform>(camEnt);
	pTransform->rotation.y = 0;
	ECS::Components::Camera compV{};
	g_pEntityManager->AddComponent(camEnt, compV);

	{

		auto lightEnt = g_pEntityManager->CreateEntity(DirectX::XMFLOAT3(0, 4, 0));
		ECS::Components::Light compL{};
		compL.color = mathstl::Vector4(1, 1, 0, 1);
		g_pEntityManager->AddComponent(lightEnt, compL);
	}
	{

		auto lightEnt = g_pEntityManager->CreateEntity(DirectX::XMFLOAT3(-1,-1, 0));
		ECS::Components::Light compL{};
		compL.color = mathstl::Vector4(1, 1, 0, 1);
		g_pEntityManager->AddComponent(lightEnt, compL);
	}
	{

		auto lightEnt = g_pEntityManager->CreateEntity(DirectX::XMFLOAT3(1,-1,-1));
		ECS::Components::Light compL{};
		compL.color = mathstl::Vector4(1, 1, 0, 1);
		g_pEntityManager->AddComponent(lightEnt, compL);
	}


	g_pApplicationState->RegisterUpdateFunction([camEnt](ApplicationState& state) { state.selectedEntities.push_back(camEnt); state.mainCameraEntity = camEnt; });
	m_applicationState.ProcessStateUpdates();
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
		WaitForRendererToFinish(1);


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

void Application::WaitForRendererToFinish(u32 idx)
{
	g_mainRenderThreadSyncSemaphore.Post();
	g_frameTimerSemaphore.Wait();
}
