#include <glfw3/glfw3.h>
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Rendering/Vulkan/VulkanBackend.h"
#include "Rendering/RenderLayer.h"
#include "Rendering/Core/Shader.h"
#include "Rendering/Core/Pipeline.h"
#include "Rendering/Core/Attachment.h"
#include "Rendering/Core/RenderPass.h"
#include "WindowManager.h"
#include "Core/UI/UIManager.h"
#include "TimeData.h"
#include "Application.h"

Application::Application(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title)
	: m_renderLayer{}
{
	g_pWindowManager = stltype::make_unique<WindowManager>(screenWidth, screenHeight, title);
	bool bCanRender = m_renderLayer.InitRenderLayer(screenWidth, screenHeight, title);
	m_ui.Setup(!bCanRender);

	auto mainVert = Shader("Shaders/Simple.vert.spv", "main");
	auto mainFrag = Shader("Shaders/Simple.frag.spv", "main");
	ColorAttachmentInfo colorAttachmentInfo{};
	colorAttachmentInfo.format = TexFormat::SWAPCHAIN;
	auto colorAttachment = RenderPassAttachmentColor::CreateColorAttachment(colorAttachmentInfo);
	auto renderPass = RenderPass::CreateFullScreenRenderPassSimple(colorAttachment);
	PSO mainPSO = PSO::CreatePipeline(mainVert, mainFrag, PipelineVertexInputs{}, PipelineInfo{}, renderPass);
}

Application::~Application()
{
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
}

void Application::Update()
{
	//float dt = m_time.Step();
}

void Application::Render()
{

	m_ui.DrawUI(0.16f, LogData::Get()->GetApplicationInfos());
}

void Application::ConsolePrintDebug() const
{
	m_ui.PrintConsoleLog();
}
