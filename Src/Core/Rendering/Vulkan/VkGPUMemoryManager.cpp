#include "VkGPUMemoryManager.h"
#include "BackendDefines.h"
#include "VkGlobals.h"

GPUMemManager<Vulkan>::~GPUMemManager()
{
    for (auto& handle : m_memoryHandles)
    {
        UnmapMemory(handle);
        vkFreeMemory(VK_LOGICAL_DEVICE, handle, VulkanAllocator());
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

    m_memoryHandles.push_back(memory);

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

GPUMappedMemoryHandle GPUMemManager<Vulkan>::MapMemory(GPUMemoryHandle memory, size_t size)
{
    DEBUG_ASSERT(std::find(m_memoryHandles.begin(), m_memoryHandles.end(), memory) != m_mappedMemoryHandles.end());
    GPUMappedMemoryHandle data;
    vkMapMemory(VK_LOGICAL_DEVICE, memory, 0, size, 0, &data);
    m_mappedMemoryHandles.push_back(memory);
    return data;
}

void GPUMemManager<Vulkan>::UnmapMemory(GPUMemoryHandle memory)
{
	auto it = std::find(m_mappedMemoryHandles.begin(), m_mappedMemoryHandles.end(), memory);
    if (it == m_mappedMemoryHandles.end())
        return;
	vkUnmapMemory(VK_LOGICAL_DEVICE, *it);
	m_mappedMemoryHandles.erase(it);
}

void GPUMemManager<Vulkan>::TryFreeMemory(GPUMemoryHandle memoryHandle)
{
    if(memoryHandle != VK_NULL_HANDLE)
    {
        UnmapMemory(memoryHandle);
        vkFreeMemory(VK_LOGICAL_DEVICE, memoryHandle, VulkanAllocator());
        m_memoryHandles.erase(std::find(m_memoryHandles.begin(), m_memoryHandles.end(), memoryHandle));
    }
}
