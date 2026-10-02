#define VMA_IMPLEMENTATION
#include "ConvAllocatorSimple.h"
#include "Core/Rendering/Core/GPUMemoryManager.h"
#include "Utils/VkEnumHelpers.h"
#include "VkBackendAccess.h"
#include "vk_mem_alloc.h"

void ConvAllocatorSimple::Init()
{
    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo createInfo = {};
    createInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    createInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    createInfo.physicalDevice = VkBackend::PhysicalDevice();
    createInfo.device = VkBackend::Device();
    createInfo.instance = VkBackend::Instance();
    createInfo.pVulkanFunctions = &vulkanFunctions;
    // Smaller than VMA's 256 MB default so the memory window shows buckets filling up
    createInfo.preferredLargeHeapBlockSize = 64ull * 1024 * 1024;
    vmaCreateAllocator(&createInfo, &m_vma);
}

ConvAllocatorSimple::~ConvAllocatorSimple()
{
    for (const auto& [memory, entry] : m_entries)
        Destroy(memory, entry);
    m_entries.clear();
    vmaDestroyAllocator(m_vma);
}

GPUMemoryHandle ConvAllocatorSimple::AllocateBuffer(BufferUsage usage,
                                                    const VkBufferCreateInfo& info,
                                                    VkBuffer& outBuffer,
                                                    void*& outMapped)
{
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = Conv2VmaMemFlags(usage);
    // Host buffers stay mapped for their whole life so callers write straight through the pointer
    if (allocInfo.usage == VMA_MEMORY_USAGE_AUTO_PREFER_HOST)
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocation memory{VK_NULL_HANDLE};
    VmaAllocationInfo resultInfo{};
    const VkResult result = vmaCreateBuffer(m_vma, &info, &allocInfo, &outBuffer, &memory, &resultInfo);
    DEBUG_ASSERT(result == VK_SUCCESS);
    outMapped = resultInfo.pMappedData;

    SimpleScopedGuard<CustomMutex> lock(m_mutex);
    m_entries[memory] = {outBuffer, VK_NULL_HANDLE};
    return memory;
}

GPUMemoryHandle ConvAllocatorSimple::AllocateImage(const VkImageCreateInfo& info, VkImage& outImage)
{
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VmaAllocation memory{VK_NULL_HANDLE};
    const VkResult result = vmaCreateImage(m_vma, &info, &allocInfo, &outImage, &memory, nullptr);
    DEBUG_ASSERT(result == VK_SUCCESS);

    SimpleScopedGuard<CustomMutex> lock(m_mutex);
    m_entries[memory] = {VK_NULL_HANDLE, outImage};
    return memory;
}

void ConvAllocatorSimple::Free(GPUMemoryHandle memory)
{
    SimpleScopedGuard<CustomMutex> lock(m_mutex);
    const auto it = m_entries.find(memory);
    if (it == m_entries.end())
        return;
    Destroy(memory, it->second);
    m_entries.erase(it);
}

void ConvAllocatorSimple::SetName(GPUMemoryHandle memory, const stltype::string& name)
{
    // Under the lock because the stats snapshot reads the name back
    SimpleScopedGuard<CustomMutex> lock(m_mutex);
    vmaSetAllocationName(m_vma, memory, name.c_str());
}

u64 ConvAllocatorSimple::GetUsedVram()
{
    const VkPhysicalDeviceMemoryProperties& memProps = VkBackend::MemoryProperties();
    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(m_vma, budgets);

    u64 used = 0;
    for (u32 i = 0; i < memProps.memoryHeapCount; ++i)
    {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            used += budgets[i].usage;
    }
    return used;
}

void ConvAllocatorSimple::FillStats(GPUMemoryStats& out)
{
    const VkPhysicalDeviceMemoryProperties& memProps = VkBackend::MemoryProperties();

    VmaBudget budgets[VK_MAX_MEMORY_HEAPS];
    vmaGetHeapBudgets(m_vma, budgets);
    out.heaps.clear();
    for (u32 i = 0; i < memProps.memoryHeapCount; ++i)
    {
        const bool deviceLocal = memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
        out.heaps.push_back({budgets[i].usage, budgets[i].budget, deviceLocal});
    }

    VmaTotalStatistics total{};
    vmaCalculateStatistics(m_vma, &total);
    out.types.clear();
    for (u32 i = 0; i < memProps.memoryTypeCount; ++i)
    {
        const VmaStatistics& stats = total.memoryType[i].statistics;
        if (stats.blockCount == 0)
            continue;
        const VkMemoryPropertyFlags flags = memProps.memoryTypes[i].propertyFlags;
        out.types.push_back({i,
                             memProps.memoryTypes[i].heapIndex,
                             (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0,
                             (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0,
                             (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0,
                             (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0,
                             stats.blockCount,
                             stats.allocationCount,
                             stats.blockBytes,
                             stats.allocationBytes});
    }

    SimpleScopedGuard<CustomMutex> lock(m_mutex);
    out.allocations.clear();
    out.allocations.reserve(m_entries.size());
    for (const auto& [memory, entry] : m_entries)
    {
        VmaAllocationInfo info{};
        vmaGetAllocationInfo(m_vma, memory, &info);
        out.allocations.push_back({info.pName ? info.pName : "",
                                   info.size,
                                   info.memoryType,
                                   entry.image != VK_NULL_HANDLE,
                                   info.pMappedData != nullptr});
    }
}

void ConvAllocatorSimple::DumpStatsJson(const char* pPath)
{
    // Same JSON VMA's GpuMemDumpVis.py reads
    SimpleScopedGuard<CustomMutex> lock(m_mutex);
    char* pJson = nullptr;
    vmaBuildStatsString(m_vma, &pJson, VK_TRUE);
    FILE* pFile = fopen(pPath, "w");
    fputs(pJson, pFile);
    fclose(pFile);
    vmaFreeStatsString(m_vma, pJson);
    DEBUG_LOGF("[ConvAllocatorSimple] Wrote {}", pPath);
}

void ConvAllocatorSimple::Destroy(GPUMemoryHandle memory, const Entry& entry)
{
    // Destroying a mapped allocation unmaps it too
    if (entry.buffer != VK_NULL_HANDLE)
        vmaDestroyBuffer(m_vma, entry.buffer, memory);
    else
        vmaDestroyImage(m_vma, entry.image, memory);
}
