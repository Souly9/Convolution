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

// Track right mouse state and last cursor pos to generate delta
static bool s_rightButtonDown = false;
static double s_lastCursorX = 0.0, s_lastCursorY = 0.0;

void InputManager::RegisterInputCallbacks(GLFWwindow* pWindow)
{
	glfwSetKeyCallback(pWindow, KeyPressCallback);
	glfwSetMouseButtonCallback(pWindow, MouseButtonCallback);
	glfwSetScrollCallback(pWindow, ScrollCallback);
	glfwSetCursorPosCallback(pWindow, MouseMoveCallback);
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
	if (button == GLFW_MOUSE_BUTTON_RIGHT)
	{
		if (action == GLFW_PRESS)
		{
			s_rightButtonDown = true;
			s_lastCursorX = xPos;
			s_lastCursorY = yPos;
			g_pEventSystem->OnRightMouseClick({ xPos, yPos, DirectX::XMFLOAT2(0.0f, 0.0f), true });
		}
		else if (action == GLFW_RELEASE)
		{
			s_rightButtonDown = false;
			g_pEventSystem->OnRightMouseClick({ xPos, yPos, DirectX::XMFLOAT2(0.0f, 0.0f), false });
		}
	}
}

void InputManager::MouseMoveCallback(GLFWwindow* window, f64 xpos, f64 ypos)
{
	if (IsForImGui())
	{
		return;
	}

	// compute delta from last cursor
	double dx = xpos - s_lastCursorX;
	double dy = ypos - s_lastCursorY;

	// update last cursor always
	s_lastCursorX = xpos;
	s_lastCursorY = ypos;

	// if right button held, send right mouse event with delta
	if (s_rightButtonDown)
	{
		RightMouseClickEventData data;
		data.mousePosX = xpos;
		data.mousePosY = ypos;
		data.mouseDelta = DirectX::XMFLOAT2((float)dx, (float)dy);
		data.pressed = true;
		g_pEventSystem->OnRightMouseClick(data);
	}
	else
	{
		// Optionally send a generic mouse move event
		MouseMoveEventData mdata;
		mdata.mousePos = DirectX::XMUINT2((u32)xpos, (u32)ypos);
		mdata.mouseDelta = DirectX::XMFLOAT2((float)dx, (float)dy);
		g_pEventSystem->OnMouseMove(mdata);
	}
}

void InputManager::ScrollCallback(GLFWwindow* window, f64 xoffset, f64 yoffset)
{
	if (IsForImGui())
	{
		return;
	}
	g_pEventSystem->OnScroll({ mathstl::Vector2((f32)xoffset, (f32)yoffset) });
}
