#include "MtlGPUMemoryManager.h"
#include "Core/Rendering/Core/Texture.h"

// TODO(Metal): heap-based allocation; recommendedMaxWorkingSetSize/currentAllocatedSize for stats

void GPUMemManager<Metal>::Init(Allocator allocatorMode)
{
    m_allocatorMode = allocatorMode;
    m_isInitialized = true;
}

GPUMemManager<Metal>::~GPUMemManager()
{
}

MTL::Buffer* GPUMemManager<Metal>::AllocateBuffer(BufferUsage usage, u64 size)
{
    return nullptr;
}

MTL::Texture* GPUMemManager<Metal>::AllocateTexture(const TextureInfo& info)
{
    return nullptr;
}

GPUMappedMemoryHandle GPUMemManager<Metal>::MapMemory(MTL::Buffer* pBuffer)
{
    return nullptr;
}

u64 GPUMemManager<Metal>::GetUsedVram()
{
    return 0;
}
