#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Backend/BackendForwardDecls.h"

// Snapshot for the memory window; the allocator's memory blocks are the buckets
struct GPUMemoryStats
{
    struct Heap
    {
        u64 usage;
        u64 budget;
        bool deviceLocal;
    };
    struct Type
    {
        u32 index;
        u32 heapIndex;
        bool deviceLocal;
        bool hostVisible;
        bool hostCoherent;
        bool hostCached;
        u32 blockCount;
        u32 allocationCount;
        u64 blockBytes;
        u64 allocationBytes;
    };
    struct Allocation
    {
        stltype::string name;
        u64 size;
        u32 memoryType;
        bool isImage;
        bool isMapped;
    };
    stltype::vector<Heap> heaps;
    stltype::vector<Type> types; // only types that own at least one block
    stltype::vector<Allocation> allocations;
};

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/ConvAllocatorSimple.h"
#elif defined(USE_METAL)
#include "Core/Rendering/Metal/MtlGPUMemoryManager.h"
#endif
