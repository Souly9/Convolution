#include <glfw3/glfw3.h>
#include <EASTL/string_view.h>
#include "GlobalDefines.h"
#include "Rendering/Backend/VulkanBackend.h"
#include "Rendering/RenderLayer.h"
#include "WindowManager.h"
#include "Core/UI/UIManager.h"
#include "TimeData.h"
#include "Application.h"

Application::Application(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title)
	: m_windowManager{ screenWidth, screenHeight, title }, m_renderLayer{}
{
	bool bCanRender = m_renderLayer.InitRenderLayer(screenWidth, screenHeight, title);
	m_ui.Setup(!bCanRender);
}

void Application::Run()
{
	while(!glfwWindowShouldClose(m_windowManager.GetWindow()))
	{
		Update();
		
		Render();

		glfwPollEvents();
	}
}

void Application::Update()
{
	float dt = m_time.Step();
}

void Application::Render()
{

	m_ui.DrawUI(0.16f, UIData::Get()->GetApplicationInfos());
}
