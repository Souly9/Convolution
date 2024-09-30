#include "VkGPUMemoryManager.h"
#include "BackendDefines.h"
#include "VkGlobals.h"

GPUMemManager<Vulkan>::~GPUMemManager()
{
    for (auto& handle : m_memoryHandles)
    {
        TryFreeMemory(handle);
    }
}

GPUMemoryHandle GPUMemManager<Vulkan>::AllocateMemory(size_t size, VkMemoryPropertyFlags properties, VkMemoryRequirements requirements)
{
    GPUMemoryHandle memory;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = GetMemoryTypeIndex(properties, requirements.memoryTypeBits);

    DEBUG_ASSERT(vkAllocateMemory(VK_LOGICAL_DEVICE, &allocInfo, nullptr, &memory) == VK_SUCCESS);

    return memory;
}

u32 GPUMemManager<Vulkan>::GetMemoryTypeIndex(VkMemoryPropertyFlags properties, u32 filter)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(VK_PHYS_DEVICE, &memProperties); 

    for (u32 i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((filter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    DEBUG_ASSERT(false);
    return 0;
}

void GPUMemManager<Vulkan>::TryFreeMemory(GPUMemoryHandle memoryHandle)
{
    vkFreeMemory(VK_LOGICAL_DEVICE, memoryHandle, VulkanAllocator());
}
