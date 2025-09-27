#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "VkGPUMemoryManager.h"
#include "BackendDefines.h"
#include "VkGlobals.h"
#include "Utils/VkEnumHelpers.h"

// Don't want to include the VMA header in the header file, so we define it here
static inline VmaAllocator s_vmaAllocator;

struct DetailedAllocationData
{
    GPUMemoryHandle memoryHandle;
    VmaAllocation vmaAllocation;
	VkBuffer bufferHandle{ VK_NULL_HANDLE };
    VkImage imageHandle{ VK_NULL_HANDLE };
};
stltype::vector<DetailedAllocationData> s_memoryHandles{};

inline bool NeedsMappableHandle(const BufferUsage& m)
{
    switch (m)
    {
        case BufferUsage::Staging:
        case BufferUsage::Uniform:
        case BufferUsage::SSBOHost:
        case BufferUsage::IndirectDrawCmds:
            return true;
    }
    return false;
}

void GPUMemManager<Vulkan>::Init(Allocator allocatorMode)
{
	m_allocatorMode = allocatorMode;
    if(allocatorMode == Allocator::VMA)
    {
        InitializeVMA();
	}
    else if (allocatorMode == Allocator::Default)
    {
        // No initialization needed for default allocator
    }
    else
    {
        DEBUG_ASSERT(false);
	}
}

void GPUMemManager<Vulkan>::FreeMemory(GPUMemoryHandle memory)
{
    const auto it = std::find_if(s_memoryHandles.begin(), s_memoryHandles.end(), [&memory](const auto& elem)
        {
            return elem.memoryHandle == memory;
        });
    if (it->bufferHandle != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(s_vmaAllocator, it->bufferHandle, it->vmaAllocation);
    }
    else if (it->imageHandle != VK_NULL_HANDLE)
    {
        vmaDestroyImage(s_vmaAllocator, it->imageHandle, it->vmaAllocation);
    }
    s_memoryHandles.erase(it);
}

void GPUMemManager<Vulkan>::InitializeVMA()
{
    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorCreateInfo.physicalDevice = VK_PHYS_DEVICE;
    allocatorCreateInfo.device = VK_LOGICAL_DEVICE;
    allocatorCreateInfo.instance = VkGlobals::GetContext().Instance;

    vmaCreateAllocator(&allocatorCreateInfo, &s_vmaAllocator);
}

GPUMemManager<Vulkan>::~GPUMemManager()
{
    m_allocatinggMutex.Lock();
    for (auto& handle : s_memoryHandles)
    {
        if (m_allocatorMode != Allocator::VMA)
        {
            /*
            UnmapMemory(handle);
            vkFreeMemory(VK_LOGICAL_DEVICE, handle, VulkanAllocator());
            */
            DEBUG_ASSERT(false);
		}
        if (handle.bufferHandle != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(s_vmaAllocator, handle.bufferHandle, handle.vmaAllocation);
        }
        else if (handle.imageHandle != VK_NULL_HANDLE)
        {
            vmaDestroyImage(s_vmaAllocator, handle.imageHandle, handle.vmaAllocation);
        }
		DEBUG_ASSERT(handle.imageHandle == VK_NULL_HANDLE && handle.bufferHandle == VK_NULL_HANDLE);
    }
	FreeVMA();
    m_allocatinggMutex.Unlock();
}

GPUMemoryHandle GPUMemManager<Vulkan>::AllocateMemory(size_t size, VkMemoryPropertyFlags properties, VkMemoryRequirements requirements)
{
    DEBUG_ASSERT(false);

    //DetailedAllocationData allocation;

    //VkMemoryAllocateInfo allocInfo{};
    //allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    //allocInfo.allocationSize = requirements.size;
    //allocInfo.memoryTypeIndex = GetMemoryTypeIndex(properties, requirements.memoryTypeBits);

    //m_allocatinggMutex.Lock();
    //DEBUG_ASSERT(vkAllocateMemory(VK_LOGICAL_DEVICE, &allocInfo, nullptr, &allocation.memoryHandle.memory) == VK_SUCCESS);

    //s_memoryHandles.push_back(allocation);
    //m_allocatinggMutex.Unlock();

    //return memory;
    return VK_NULL_HANDLE;
}

GPUMemoryHandle GPUMemManager<Vulkan>::AllocateBuffer(BufferUsage usage, VkBufferCreateInfo bufferInfo, VkBuffer& bufferToCreate)
{
    if(m_allocatorMode != Allocator::VMA)
    {
        DEBUG_ASSERT(false);
	}
    m_allocatinggMutex.Lock();
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = Conv2VmaMemFlags(usage);
    allocInfo.flags = NeedsMappableHandle(usage)? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT : 0;

    DetailedAllocationData allocation;
    DEBUG_ASSERT(vmaCreateBuffer(s_vmaAllocator, &bufferInfo, &allocInfo, &allocation.bufferHandle, &allocation.vmaAllocation, nullptr) == VK_SUCCESS);
	bufferToCreate = allocation.bufferHandle;
	allocation.memoryHandle = allocation.vmaAllocation; // Store the allocation handle
    s_memoryHandles.push_back(allocation);
    m_allocatinggMutex.Unlock();
	return allocation.vmaAllocation;
}

GPUMemoryHandle GPUMemManager<Vulkan>::AllocateImage(VkImageCreateInfo imageInfo, VkImage& imageToCreate)
{
    if (m_allocatorMode != Allocator::VMA)
    {
        DEBUG_ASSERT(false);
    }
    m_allocatinggMutex.Lock();
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    DetailedAllocationData allocation;
	DEBUG_ASSERT(vmaCreateImage(s_vmaAllocator, &imageInfo, &allocInfo, &allocation.imageHandle, &allocation.vmaAllocation, nullptr) == VK_SUCCESS);
	imageToCreate = allocation.imageHandle;
    allocation.memoryHandle = allocation.vmaAllocation; // Store the allocation handle
    s_memoryHandles.push_back(allocation);
    m_allocatinggMutex.Unlock();
    return allocation.vmaAllocation;
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
    m_allocatinggMutex.Lock();
    m_mappingMutex.Lock();
    const auto it = std::find_if(s_memoryHandles.begin(), s_memoryHandles.end(), [&memory](const auto& elem)
        {
            return elem.memoryHandle == memory;
        });
    DEBUG_ASSERT(it != s_memoryHandles.end());
    GPUMappedMemoryHandle data;
	DEBUG_ASSERT(vmaMapMemory(s_vmaAllocator, it->vmaAllocation, &data) == VK_SUCCESS);
    m_mappedMemoryHandles.push_back(memory);

    m_mappingMutex.Unlock();
    m_allocatinggMutex.Unlock();
    return data;
}

void GPUMemManager<Vulkan>::UnmapMemory(GPUMemoryHandle memory)
{
    m_mappingMutex.Lock();
    auto mapped_it = std::find(m_mappedMemoryHandles.begin(), m_mappedMemoryHandles.end(), memory);

    if (mapped_it != m_mappedMemoryHandles.end())
    {
        //vkUnmapMemory(VK_LOGICAL_DEVICE, *it);
        vmaUnmapMemory(s_vmaAllocator, *mapped_it);
        m_mappedMemoryHandles.erase(mapped_it);
    }
    m_mappingMutex.Unlock();
}

void GPUMemManager<Vulkan>::TryFreeMemory(GPUMemoryHandle memory)
{
    m_allocatinggMutex.Lock();
    if(memory != VK_NULL_HANDLE)
    {
        UnmapMemory(memory);
        //vkFreeMemory(VK_LOGICAL_DEVICE, memoryHandle, VulkanAllocator());
        FreeMemory(memory);
    }
    m_allocatinggMutex.Unlock();
}

void GPUMemManager<Vulkan>::BindImageMemory(GPUMemoryHandle handle)
{
    m_allocatinggMutex.Lock();
    const auto it = std::find_if(s_memoryHandles.begin(), s_memoryHandles.end(), [&handle](const auto& elem)
        {
            return elem.memoryHandle == handle;
        });
	DEBUG_ASSERT(vmaBindImageMemory(s_vmaAllocator, it->vmaAllocation, it->imageHandle) == VK_SUCCESS);
    m_allocatinggMutex.Unlock();
}

void GPUMemManager<Vulkan>::FreeVMA()
{
    vmaDestroyAllocator(s_vmaAllocator);
}
