#pragma once
#include "Core/Global/GlobalDefines.h"

// Isn't responsible for freeing or managing the globals, just provides access
class VkGlobals
{
public:
	static VkDevice GetLogicalDevice();
	static VkFormat GetSwapChainImageFormat();
	static VkExtent2D GetSwapChainExtent();

	static void SetLogicalDevice(VkDevice physDevice);
	static void SetSwapChainImageFormat(VkFormat physDevice);
	static void SetSwapChainExtent(VkExtent2D physDevice);
private:
	static VkDevice s_logicalDevice;
	static VkFormat s_swapChainImageFormat;
	static VkExtent2D s_swapChainExtent;
};