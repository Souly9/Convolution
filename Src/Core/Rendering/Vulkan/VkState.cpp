#include "VkState.h"
#include "VkGPUMemoryManager.h"
#include "VkTextureManager.h"
#include "VkTracyManager.h"

stltype::unique_ptr<GPUMemManager<Vulkan>> VkState::pGPUMemoryManager = stltype::make_unique<GPUMemManager<Vulkan>>();
stltype::unique_ptr<VkTracyGPUManager> VkState::pTracyGPUManager = stltype::make_unique<VkTracyGPUManager>();

VkDevice VkState::s_logicalDevice = nullptr;
VkProfiler* VkState::s_pProfiler = nullptr;
VkTracyGPUManager* VkState::s_pTracyManager = nullptr;
VkSwapchainKHR VkState::s_mainSwapChain{};
VkQueue VkState::s_presentQueue{};
VkQueue VkState::s_graphicsQueue{};
stltype::vector<Texture*> VkState::s_swapChainImages{};
QueueFamilyIndices VkState::s_indices{};
Queues VkState::s_queues{};
VkPhysicalDevice VkState::s_physicalDevice = VK_NULL_HANDLE;
VkPhysicalDeviceProperties VkState::s_physicalDeviceProperties{};
VkPhysicalDeviceMemoryProperties VkState::s_physicalDeviceMemoryProperties{};
u64 VkState::s_totalVram = 0;
Texture* VkState::s_pDepthStencilBuffer = nullptr;
VulkanContext VkState::s_context{};

VkDevice VkState::GetLogicalDevice()
{
    DEBUG_ASSERT(s_logicalDevice != nullptr);
    return s_logicalDevice;
}

VkProfiler* VkState::GetProfiler()
{
    return s_pProfiler;
}

VkTracyGPUManager* VkState::GetTracyManager()
{
    return s_pTracyManager ? s_pTracyManager : VkState::pTracyGPUManager.get();
}

VkSwapchainKHR VkState::GetMainSwapChain()
{
    return s_mainSwapChain;
}

VkQueue VkState::GetPresentQueue()
{
    return s_presentQueue;
}

VkQueue VkState::GetGraphicsQueue()
{
    return s_graphicsQueue;
}

Queues VkState::GetAllQueues()
{
    return s_queues;
}

const stltype::vector<Texture*>& VkState::GetSwapChainImages()
{
    return s_swapChainImages;
}

QueueFamilyIndices VkState::GetQueueFamilyIndices()
{
    return s_indices;
}

VkPhysicalDevice VkState::GetPhysicalDevice()
{
    DEBUG_ASSERT(s_physicalDevice != nullptr);
    return s_physicalDevice;
}

const VkPhysicalDeviceProperties& VkState::GetPhysicalDeviceProperties()
{
    return s_physicalDeviceProperties;
}

const VkPhysicalDeviceMemoryProperties& VkState::GetPhysicalDeviceMemoryProperties()
{
    return s_physicalDeviceMemoryProperties;
}

u64 VkState::GetTotalVram()
{
    return s_totalVram;
}

Texture* VkState::GetDepthStencilBuffer()
{
    return s_pDepthStencilBuffer;
}

void VkState::SetContext(const VulkanContext& context)
{
    s_context = context;
}

void VkState::SetProfiler(VkProfiler* pProfiler)
{
    s_pProfiler = pProfiler;
}

void VkState::SetTracyManager(VkTracyGPUManager* pTracyMgr)
{
    s_pTracyManager = pTracyMgr;
}

void VkState::SetPhysicalDeviceProperties(const VkPhysicalDeviceProperties& physDeviceProps)
{
    s_physicalDeviceProperties = physDeviceProps;
}

void VkState::SetPhysicalDeviceMemoryProperties(const VkPhysicalDeviceMemoryProperties& memProps)
{
    s_physicalDeviceMemoryProperties = memProps;
    s_totalVram = 0;
    for (u32 i = 0; i < memProps.memoryHeapCount; ++i)
    {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        {
            s_totalVram += memProps.memoryHeaps[i].size;
        }
    }
}

void VkState::SetLogicalDevice(VkDevice physDevice)
{
    s_logicalDevice = physDevice;
}

void VkState::SetMainSwapChain(const VkSwapchainKHR swapChain)
{
    s_mainSwapChain = swapChain;
}

void VkState::SetPresentQueue(const VkQueue presentQueue)
{
    s_presentQueue = presentQueue;
}

void VkState::SetGraphicsQueue(const VkQueue graphicsQueue)
{
    s_graphicsQueue = graphicsQueue;
}

void VkState::SetAllQueues(const Queues& queues)
{
    s_queues = queues;
}

void VkState::SetSwapChainImages(const stltype::vector<Texture*>& images)
{
    s_swapChainImages = images;
}

void VkState::SetQueueFamilyIndices(const QueueFamilyIndices& indices)
{
    s_indices = indices;
}

void VkState::SetPhysicalDevice(const VkPhysicalDevice& physDevice)
{
    s_physicalDevice = physDevice;
}

void VkState::SetDepthStencilBuffer(Texture* pDepthTex)
{
    s_pDepthStencilBuffer = pDepthTex;
}

const VulkanContext& VkState::GetContext()
{
    return s_context;
}
