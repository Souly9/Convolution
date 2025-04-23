#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include "InputManager.h"

struct KeyInfo
{
	KeyType key;
	bool wasPressedLastFrame{ false };
};;
static stltype::hash_map<s32, KeyInfo> s_keyMap = {
	{ GLFW_KEY_W, {KeyType::ForwardMove} },
	{ GLFW_KEY_A, {KeyType::LeftMove} },
	{ GLFW_KEY_S, {KeyType::BackwardMove} },
	{ GLFW_KEY_D, {KeyType::RightMove} }
};

static bool IsForImGui()
{
	auto& io = ImGui::GetIO();
	if (io.WantCaptureKeyboard || io.WantCaptureMouse)
	{
		return true;
	}
	return false;
}

void InputManager::RegisterInputCallbacks(GLFWwindow* pWindow)
{
	glfwSetKeyCallback(pWindow, KeyPressCallback);
	glfwSetMouseButtonCallback(pWindow, MouseButtonCallback);
	glfwSetScrollCallback(pWindow, ScrollCallback);
	g_pEventSystem->AddUpdateEventCallback([](const UpdateEventData& d)
		{
			for (auto& [key, keyInfo] : s_keyMap)
			{
				if (keyInfo.wasPressedLastFrame)
				{
					g_pEventSystem->OnKeyHold({ keyInfo.key });
				}
			}
		});
}

void InputManager::KeyPressCallback(GLFWwindow* window, s32 key, s32 scancode, s32 action, s32 mods)
{
	if (IsForImGui())
	{
		return;
	}
	auto pFoundKeyIterator = s_keyMap.find(key);
	const bool isSupportedKey = pFoundKeyIterator != s_keyMap.end();
	if (!isSupportedKey)
		return;
	auto& keyInfo = pFoundKeyIterator->second;
	if(action == GLFW_PRESS)
	{
		keyInfo.wasPressedLastFrame = true;
		g_pEventSystem->OnKeyPress({ keyInfo.key });
	}
	else if (action == GLFW_RELEASE)
	{
		keyInfo.wasPressedLastFrame = false;
	}
}

void InputManager::MouseButtonCallback(GLFWwindow* window, s32 button, s32 action, s32 mods)
{
	if (IsForImGui())
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

void InputManager::ScrollCallback(GLFWwindow* window, f64 xoffset, f64 yoffset)
{
	if (IsForImGui())
	{
		return;
	}
	g_pEventSystem->OnScroll({ mathstl::Vector2((f32)xoffset, (f32)yoffset) });
}
