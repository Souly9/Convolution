#pragma once
#include "BackendDefines.h"
#include "Core/Global/ThreadBase.h"
#include "Core/Rendering/Core/Buffer.h"

struct GPUMemoryStats;

// VMA's memory blocks are the buckets; this tracks every allocation and keeps host memory mapped
class ConvAllocatorSimple
{
public:
    void Init();
    ~ConvAllocatorSimple();

    // outMapped stays valid until Free for host-visible usages, nullptr otherwise
    GPUMemoryHandle AllocateBuffer(BufferUsage usage,
                                   const VkBufferCreateInfo& info,
                                   VkBuffer& outBuffer,
                                   void*& outMapped);
    GPUMemoryHandle AllocateImage(const VkImageCreateInfo& info, VkImage& outImage);
    void Free(GPUMemoryHandle memory);
    void SetName(GPUMemoryHandle memory, const stltype::string& name);

    u64 GetUsedVram();
    // Both are called from the UI thread; VMA is internally synchronized and m_mutex guards the rest
    void FillStats(GPUMemoryStats& out);
    void DumpStatsJson(const char* pPath);

private:
    struct Entry
    {
        VkBuffer buffer{VK_NULL_HANDLE};
        VkImage image{VK_NULL_HANDLE};
    };
    void Destroy(GPUMemoryHandle memory, const Entry& entry);

    stltype::hash_map<GPUMemoryHandle, Entry> m_entries;
    CustomMutex m_mutex;
    VmaAllocator m_vma{VK_NULL_HANDLE};
};
