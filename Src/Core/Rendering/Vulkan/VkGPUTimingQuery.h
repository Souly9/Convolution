#pragma once
#include "Core/Rendering/Core/GPUTimingQuery.h"
#include <vulkan/vulkan.h>

class VkGPUTimingQuery : public GPUTimingQueryBase
{
public:
    void Init(u32 maxPasses) override;
    void Destroy() override;

    void ResetQueries(CommandBuffer* pCmdBuffer, u32 frameIdx) override;
    void ReadResults(u32 frameIdx) override;

protected:
    void WriteTimestampImpl(CommandBuffer* pCmdBuffer, u32 passIndex, bool isStart) override;

private:
    VkQueryPool m_queryPool{VK_NULL_HANDLE};
    f64 m_timestampPeriodNs{1.0};
    u32 m_maxPasses{0};
    u32 m_queryCount{0};
    stltype::vector<u64> m_timestampResults;
};
