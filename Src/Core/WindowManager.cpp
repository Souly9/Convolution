#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include "./WindowManager.h"
#include "Core/Events/EventSystem.h"
#include "Core/Input/InputManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Global/GlobalDefines.h"
#include <EASTL/string_view.h>
#include <GLFW/glfw3.h>

namespace
{
constexpr auto kSwapchainRecreationDebounce = std::chrono::milliseconds(150);

void FramebufferSizeCallback(GLFWwindow* pWindow, int width, int height)
{
    if (pWindow == nullptr)
        return;

    const uint32_t clampedWidth = static_cast<uint32_t>(stltype::max(width, 1));
    const uint32_t clampedHeight = static_cast<uint32_t>(stltype::max(height, 1));

    auto* pWindowManager = static_cast<WindowManager*>(glfwGetWindowUserPointer(pWindow));
    if (pWindowManager != nullptr)
    {
        pWindowManager->SetScreenSize(clampedWidth, clampedHeight);
        pWindowManager->QueueSwapchainRecreation(clampedWidth, clampedHeight);
    }
}
} // namespace

WindowManager::WindowManager(uint32_t width, uint32_t height, stltype::string_view title) : m_title{title}
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_pWindow = stltype::unique_ptr<GLFWwindow, DestroyglfwWin>(
        glfwCreateWindow(width, height, title.data(), nullptr, nullptr));
    m_screenWidth = width;
    m_screenHeight = height;

    glfwSetWindowUserPointer(m_pWindow.get(), this);
    glfwSetFramebufferSizeCallback(m_pWindow.get(), FramebufferSizeCallback);

    InputManager::RegisterInputCallbacks(m_pWindow.get());
    // glfwSetMouseButtonCallback(m_pWindow.get(), InputManager::OnMouseButton);
    // glfwSetCursorPosCallback(m_pWindow.get(), InputManager::OnMouseMove);
    // glfwSetScrollCallback(m_pWindow.get(), InputManager::OnMouseScroll);
    // glfwSetKeyCallback(m_pWindow.get(), InputManager::OnKeyPress);
}

void WindowManager::Close() noexcept
{
    glfwTerminate();
}

GLFWwindow* WindowManager::GetWindow() const noexcept
{
    return m_pWindow.get();
}

void WindowManager::SetScreenSize(uint32_t width, uint32_t height) noexcept
{
    m_screenWidth = width;
    m_screenHeight = height;
}

void WindowManager::QueueSwapchainRecreation(uint32_t width, uint32_t height) noexcept
{
    m_pendingScreenWidth = width;
    m_pendingScreenHeight = height;
    m_hasPendingSwapchainRecreation = true;
    m_lastResizeTime = std::chrono::steady_clock::now();
}

void WindowManager::Update() noexcept
{
    if (!m_hasPendingSwapchainRecreation)
        return;

    if (std::chrono::steady_clock::now() - m_lastResizeTime < kSwapchainRecreationDebounce)
        return;

    m_hasPendingSwapchainRecreation = false;
    m_screenWidth = m_pendingScreenWidth;
    m_screenHeight = m_pendingScreenHeight;

    if (g_pEventSystem != nullptr)
    {
        g_pEventSystem->OnSwapchainRecreation({});
    }
}
