#include <GLFW/glfw3.h>
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
}

void InputManager::KeyPressCallback(GLFWwindow* window, s32 key, s32 scancode, s32 action, s32 mods)
{
	const bool isSupportedKey = s_keyMap.find(key) != s_keyMap.end();
	if (!isSupportedKey)
		return;
	if(action == GLFW_PRESS || action == GLFW_REPEAT)
		g_pEventSystem->OnKeyPress({ s_keyMap[key] });
}
