#include <glfw3/glfw3.h>
#include "Application.h"
#include "Rendering/Passes/StaticMeshPass.h"
#include "Core/UI/UIManager.h"
#include "TimeData.h"

Application::Application(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title)
	: m_renderLayer{}
{
	g_pWindowManager = stltype::make_unique<WindowManager>(screenWidth, screenHeight, title);
	bool bCanRender = m_renderLayer.InitRenderLayer(screenWidth, screenHeight, title);
	DEBUG_ASSERT(bCanRender);

	m_ui.Setup(!bCanRender);
	CreateMainPSO();
	g_pGlobalTimeData->Reset();
}

void Application::CreateMainPSO()
{
	m_staticMeshPass = stltype::make_unique<StaticMainMeshPass>(3);
	m_staticMeshPass->SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates::Complete);
	m_staticMeshPass->CreatePipeline();

}

Application::~Application()
{
	m_staticMeshPass.reset();
	g_pTexManager.reset();
	g_pGPUMemoryManager.reset();
	m_renderLayer.CleanUp();
}

void Application::Run()
{
	while(!glfwWindowShouldClose(g_pWindowManager->GetWindow()))
	{
		ConsolePrintDebug();

		Update();
		
		Render();

		glfwPollEvents();
	}
	vkDeviceWaitIdle(VkGlobals::GetLogicalDevice());
}

void Application::Update()
{
	g_pGlobalTimeData->Step();
}

void Application::Render()
{
	m_ui.DrawUI(0.16f, LogData::Get()->GetApplicationInfos());

	m_staticMeshPass->Render();
}

void Application::ConsolePrintDebug() const
{
	m_ui.PrintConsoleLog();
}
