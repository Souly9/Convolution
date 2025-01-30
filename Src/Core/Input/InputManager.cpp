#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include "InputManager.h"

static stltype::hash_map<s32, KeyType> s_keyMap = {
	{ GLFW_KEY_W, KeyType::ForwardMove },
	{ GLFW_KEY_A, KeyType::LeftMove },
	{ GLFW_KEY_S, KeyType::BackwardMove },
	{ GLFW_KEY_D, KeyType::RightMove }
};

void InputManager::RegisterInputCallbacks(GLFWwindow* pWindow)
{
	glfwSetKeyCallback(pWindow, KeyPressCallback);
	glfwSetMouseButtonCallback(pWindow, MouseButtonCallback);
}

void InputManager::KeyPressCallback(GLFWwindow* window, s32 key, s32 scancode, s32 action, s32 mods)
{
	auto& io = ImGui::GetIO();
	if (io.WantCaptureKeyboard)
	{
		return;
	}
	const bool isSupportedKey = s_keyMap.find(key) != s_keyMap.end();
	if (!isSupportedKey)
		return;
	if(action == GLFW_PRESS || action == GLFW_REPEAT)
		g_pEventSystem->OnKeyPress({ s_keyMap[key] });
}

void InputManager::MouseButtonCallback(GLFWwindow* window, s32 button, s32 action, s32 mods)
{
	auto& io = ImGui::GetIO();
	if (io.WantCaptureMouse)
	{
		return;
	}
	double xPos, yPos;
	glfwGetCursorPos(window, &xPos, &yPos);

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		g_pEventSystem->OnLeftMouseClick({ xPos, yPos });
	}
}

void InputManager::MouseMoveCallback(GLFWwindow* window, f64 xpos, f64 ypos)
{
}
