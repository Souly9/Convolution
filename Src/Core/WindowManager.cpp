#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <glfw3/glfw3.h>
#include <EASTL/string_view.h>
#include "GlobalDefines.h"
#include "./WindowManager.h"

WindowManager::WindowManager(uint32_t width, uint32_t height, stltype::string_view title)
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	m_pWindow = std::unique_ptr<GLFWwindow, DestroyglfwWin>(glfwCreateWindow(width, height, title.data(), nullptr, nullptr));

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
