#include "MtlGPUMemoryManager.h"
#include "Core/Rendering/Core/Texture.h"

// TODO(Metal): heap-based allocation; recommendedMaxWorkingSetSize/currentAllocatedSize for stats

void MtlGPUMemoryManager::Init()
{
}

MtlGPUMemoryManager::~MtlGPUMemoryManager()
{
}

MTL::Buffer* MtlGPUMemoryManager::AllocateBuffer(BufferUsage usage, u64 size)
{
    return nullptr;
}

MTL::Texture* MtlGPUMemoryManager::AllocateTexture(const TextureInfo& info)
{
    return nullptr;
}

u64 MtlGPUMemoryManager::GetUsedVram()
{
    return 0;
}

void MtlGPUMemoryManager::FillStats(GPUMemoryStats& out)
{
    // TODO(metal): heap/allocation stats for the memory window
}

void MtlGPUMemoryManager::DumpStatsJson(const char* pPath)
{
    // TODO(metal)
}
