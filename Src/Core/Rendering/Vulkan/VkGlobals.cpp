#include "VkGlobals.h"
#include "VkGPUMemoryManager.h"
#include "VkTextureManager.h"

VkDevice VkGlobals::s_logicalDevice = nullptr;
VkProfiler* VkGlobals::s_pProfiler = nullptr;
VkFormat VkGlobals::s_swapChainImageFormat{};
VkSwapchainKHR VkGlobals::s_mainSwapChain{};
VkQueue VkGlobals::s_presentQueue{};
VkQueue VkGlobals::s_graphicsQueue{};
stltype::vector<Texture*> VkGlobals::s_swapChainImages{};
QueueFamilyIndices VkGlobals::s_indices{};
Queues VkGlobals::s_queues{};
VkPhysicalDevice VkGlobals::s_physicalDevice = VK_NULL_HANDLE;
VkPhysicalDeviceProperties VkGlobals::s_physicalDeviceProperties{};
stltype::unique_ptr<GPUMemManager<Vulkan>> g_pGPUMemoryManager = stltype::make_unique<GPUMemManager<Vulkan>>();
Texture* VkGlobals::s_pDepthStencilBuffer = nullptr;
VulkanContext VkGlobals::s_context{};

VkDevice VkGlobals::GetLogicalDevice()
{
    DEBUG_ASSERT(s_logicalDevice != nullptr);
    return s_logicalDevice;
}

VkProfiler* VkGlobals::GetProfiler()
{
    //DEBUG_ASSERT(s_pProfiler != nullptr);
    return s_pProfiler;
}

VkFormat VkGlobals::GetSwapChainImageFormat()
{
    return s_swapChainImageFormat;
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

const stltype::vector<Texture*>& VkGlobals::GetSwapChainImages()
{
    return s_swapChainImages;
}

QueueFamilyIndices VkGlobals::GetQueueFamilyIndices()
{
    return s_indices;
}

VkPhysicalDevice VkGlobals::GetPhysicalDevice()
{
    DEBUG_ASSERT(s_physicalDevice != nullptr);
    return s_physicalDevice;
}

const VkPhysicalDeviceProperties& VkGlobals::GetPhysicalDeviceProperties()
{
    return s_physicalDeviceProperties;
}

Texture* VkGlobals::GetDepthStencilBuffer()
{
    return s_pDepthStencilBuffer;
}

void VkGlobals::SetContext(const VulkanContext& context)
{
    s_context = context;
}

void VkGlobals::SetProfiler(VkProfiler* pProfiler)
{
    s_pProfiler = pProfiler;
}

void VkGlobals::SetPhysicalDeviceProperties(const VkPhysicalDeviceProperties& physDeviceProps)
{
    s_physicalDeviceProperties = physDeviceProps;
}

void VkGlobals::SetLogicalDevice(VkDevice physDevice)
{
    s_logicalDevice = physDevice;
}

void VkGlobals::SetSwapChainImageFormat(VkFormat imageFormat)
{
    s_swapChainImageFormat = imageFormat;
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

void VkGlobals::SetDepthStencilBuffer(Texture* pDepthTex)
{
    s_pDepthStencilBuffer = pDepthTex;
}

const VulkanContext& VkGlobals::GetContext()
{
    return s_context;
}

void VkGlobals::SetSwapChainImages(const stltype::vector<Texture*>& images)
{
    s_swapChainImages = images;
}
