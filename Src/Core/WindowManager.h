#pragma once
#include "Core/Global/GlobalDefines.h"
#include <GLFW/glfw3.h>
#include "../Utils/MemoryUtilities.h"

struct GLFWwindow;
struct DestroyglfwWin;

class WindowManager
{
public:
	WindowManager(uint32_t width, uint32_t height, stltype::string_view title);

	GLFWwindow* GetWindow() const noexcept;

	void Close() noexcept;

	uint32_t GetScreenHeight() const noexcept { return m_screenHeight; }
	uint32_t GetScreenWidth() const noexcept { return m_screenWidth; }
	stltype::string_view GetTitle() const noexcept { return m_title; }
private:
	WindowManager() = delete;
	WindowManager(const WindowManager& copy) = delete;
	WindowManager& operator=(const WindowManager& other) = delete;

	stltype::unique_ptr<GLFWwindow, DestroyglfwWin> m_pWindow{nullptr};
	stltype::string m_title;
	uint32_t m_screenHeight;
	uint32_t m_screenWidth;
};
