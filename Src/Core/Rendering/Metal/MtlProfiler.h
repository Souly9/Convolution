#pragma once
#include "Core/Rendering/Core/Profiler.h"

// Apple GPUs have no pipeline statistics queries; only CPU-side stats would be published
class MtlProfiler : public Profiler
{
public:
    void Init() override
    {
    }
    void Destroy() override
    {
    }
    void ResetFrame(u32 frameIdx) override
    {
    }
    void PublishResults(u32 frameIdx) override
    {
    }
    u32 AllocateQuery()
    {
        return ~0u;
    }
    void AddQuery(u32 queryIdx, u32 frameIdx) override
    {
    }
    void AddCPUStats(const RendererState::SceneRenderStats& stats, u32 frameIdx) override
    {
    }
};
