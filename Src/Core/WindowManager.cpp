#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <EASTL/string_view.h>
#include "Global/GlobalDefines.h"
#include "./WindowManager.h"
#include "Core/Input/InputManager.h"

WindowManager::WindowManager(uint32_t width, uint32_t height, stltype::string_view title) : m_title{title}
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	m_pWindow = stltype::unique_ptr<GLFWwindow, DestroyglfwWin>(glfwCreateWindow(width, height, title.data(), nullptr, nullptr));

	InputManager::RegisterInputCallbacks(m_pWindow.get());
	//glfwSetFramebufferSizeCallback(m_pWindow.get(), FramebufferSizeCallback);
	//glfwSetMouseButtonCallback(m_pWindow.get(), InputManager::OnMouseButton);
	//glfwSetCursorPosCallback(m_pWindow.get(), InputManager::OnMouseMove);
	//glfwSetScrollCallback(m_pWindow.get(), InputManager::OnMouseScroll);
	//glfwSetKeyCallback(m_pWindow.get(), InputManager::OnKeyPress);
}

void WindowManager::Close() noexcept
{
	glfwTerminate();
}

GLFWwindow* WindowManager::GetWindow() const noexcept
{
	return m_pWindow.get();
}
