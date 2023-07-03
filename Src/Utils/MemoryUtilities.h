#pragma once

struct DestroyglfwWin
{
	void operator()(GLFWwindow* ptr)
	{
		glfwDestroyWindow(ptr);
	}
};
