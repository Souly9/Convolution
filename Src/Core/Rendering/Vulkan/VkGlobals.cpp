#include <vulkan/vulkan_core.h>
#include "VkGlobals.h"
#include "Core/Rendering/Vulkan/VulkanBackend.h"

VkDevice VkGlobals::s_logicalDevice = nullptr;
VkFormat VkGlobals::s_swapChainImageFormat{};
VkExtent2D VkGlobals::s_swapChainExtent{};

VkDevice VkGlobals::GetLogicalDevice()
{
	DEBUG_ASSERT(s_logicalDevice != nullptr);
	return s_logicalDevice;
}

VkFormat VkGlobals::GetSwapChainImageFormat()
{
	return s_swapChainImageFormat;
}

VkExtent2D VkGlobals::GetSwapChainExtent()
{
	return s_swapChainExtent;
}

void VkGlobals::SetLogicalDevice(VkDevice physDevice)
{
	s_logicalDevice = physDevice;
}

void VkGlobals::SetSwapChainImageFormat(VkFormat imageFormat)
{
	s_swapChainImageFormat = imageFormat;
}

void VkGlobals::SetSwapChainExtent(VkExtent2D extent)
{
	s_swapChainExtent = extent;
}
