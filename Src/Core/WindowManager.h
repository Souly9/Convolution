#pragma once
#include <glfw3/glfw3.h>
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
private:
	WindowManager() = delete;
	WindowManager(const WindowManager& copy) = delete;
	WindowManager& operator=(const WindowManager& other) = delete;

	std::unique_ptr<GLFWwindow, DestroyglfwWin> m_pWindow;
	uint32_t m_screenHeight;
	uint32_t m_screenWidth;
};
