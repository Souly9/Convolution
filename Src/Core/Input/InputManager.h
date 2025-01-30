#pragma once
#include "Core/Global/GlobalDefines.h"

class InputManager
{
public:
	static void RegisterInputCallbacks(GLFWwindow* pWindow);

	static void KeyPressCallback(GLFWwindow* window, s32 key, s32 scancode, s32 action, s32 mods);

	static void MouseButtonCallback(GLFWwindow* window, s32 button, s32 action, s32 mods);
	static void MouseMoveCallback(GLFWwindow* window, f64 xpos, f64 ypos);
};