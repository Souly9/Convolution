#include "VkGlobals.h"
#include "VkTextureManager.h"
#include "VkGPUMemoryManager.h"

VkDevice VkGlobals::s_logicalDevice = nullptr;
VkFormat VkGlobals::s_swapChainImageFormat{};
DirectX::XMUINT2 VkGlobals::s_swapChainExtent{};
VkSwapchainKHR VkGlobals::s_mainSwapChain{};
VkQueue VkGlobals::s_presentQueue{};
VkQueue VkGlobals::s_graphicsQueue{};
stltype::vector<Texture> VkGlobals::s_swapChainImages{};
QueueFamilyIndices VkGlobals::s_indices{};
Queues VkGlobals::s_queues{};
VkPhysicalDevice VkGlobals::s_physicalDevice = VK_NULL_HANDLE;
extern stltype::unique_ptr<VkTextureManager> g_pTexManager = stltype::make_unique<VkTextureManager>();
extern stltype::unique_ptr<GPUMemManager<Vulkan>> g_pGPUMemoryManager = stltype::make_unique<GPUMemManager<Vulkan>>();

VkDevice VkGlobals::GetLogicalDevice()
{
	DEBUG_ASSERT(s_logicalDevice != nullptr);
	return s_logicalDevice;
}

VkFormat VkGlobals::GetSwapChainImageFormat()
{
	return s_swapChainImageFormat;
}

DirectX::XMUINT2 VkGlobals::GetSwapChainExtent()
{
	return s_swapChainExtent;
}

VkSwapchainKHR VkGlobals::GetMainSwapChain()
{
	return s_mainSwapChain;
}

VkQueue VkGlobals::GetPresentQueue()
{
	return s_presentQueue;
}

VkQueue VkGlobals::GetGraphicsQueue()
{
	return s_graphicsQueue;
}

Queues VkGlobals::GetAllQueues()
{
	return s_queues;
}

const stltype::vector<Texture>& VkGlobals::GetSwapChainImages()
{
	return s_swapChainImages;
}

QueueFamilyIndices VkGlobals::GetQueueFamilyIndices()
{
	return s_indices;
}

VkPhysicalDevice VkGlobals::GetPhysicalDevice()
{
	return s_physicalDevice;
}

void VkGlobals::SetLogicalDevice(VkDevice physDevice)
{
	s_logicalDevice = physDevice;
}

void VkGlobals::SetSwapChainImageFormat(VkFormat imageFormat)
{
	s_swapChainImageFormat = imageFormat;
}

void VkGlobals::SetSwapChainExtent(const DirectX::XMUINT2& extent)
{
	s_swapChainExtent = extent;
}

void VkGlobals::SetMainSwapChain(const VkSwapchainKHR swapChain)
{
	s_mainSwapChain = swapChain;
}

void VkGlobals::SetPresentQueue(const VkQueue presentQueue)
{
	s_presentQueue = presentQueue;
}

void VkGlobals::SetGraphicsQueue(const VkQueue graphicsQueue)
{
	s_graphicsQueue = graphicsQueue;
}

void VkGlobals::SetAllQueues(const Queues& queues)
{
	s_queues = queues;
}

void VkGlobals::SetQueueFamilyIndices(const QueueFamilyIndices& indices)
{
	s_indices = indices;
}

void VkGlobals::SetPhysicalDevice(const VkPhysicalDevice& physDevice)
{
	s_physicalDevice = physDevice;
}

void VkGlobals::SetSwapChainImages(const stltype::vector<Texture>& images)
{
	s_swapChainImages = images;
}
