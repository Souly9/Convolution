#pragma once
#include "MtlBackendDefines.h"
#include "Core/Rendering/Core/Buffer.h"
#include "Core/Global/ThreadBase.h"

struct TextureInfo;
struct GPUMemoryStats;

// Replaces VMA: MTL::Heap sub-allocation with Shared/Private storage modes
class MtlGPUMemoryManager
{
public:
    void Init();
    ~MtlGPUMemoryManager();

    MTL::Buffer* AllocateBuffer(BufferUsage usage, u64 size);
    MTL::Texture* AllocateTexture(const TextureInfo& info);

    u64 GetUsedVram();
    void FillStats(GPUMemoryStats& out);
    void DumpStatsJson(const char* pPath);
};
