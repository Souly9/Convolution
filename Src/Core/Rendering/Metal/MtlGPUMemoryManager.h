#pragma once
#include "MtlBackendDefines.h"
#include "Core/Rendering/Core/Buffer.h"
#include "Core/Rendering/Core/GPUMemoryManager.h"
#include "Core/Global/ThreadBase.h"

struct TextureInfo;

// Replaces VMA: MTL::Heap sub-allocation with Shared/Private storage modes
template <>
class GPUMemManager<Metal>
{
public:
    void Init(Allocator allocatorMode = Allocator::Default);
    ~GPUMemManager();

    MTL::Buffer* AllocateBuffer(BufferUsage usage, u64 size);
    MTL::Texture* AllocateTexture(const TextureInfo& info);

    GPUMappedMemoryHandle MapMemory(MTL::Buffer* pBuffer);

    u64 GetUsedVram();

private:
    Allocator m_allocatorMode{Allocator::Default};
    bool m_isInitialized{false};
};
