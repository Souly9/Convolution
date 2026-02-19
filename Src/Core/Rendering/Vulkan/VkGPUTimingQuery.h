#pragma once
#include "Core/Rendering/Core/GPUTimingQuery.h"
#include "Core/Rendering/Vulkan/VkQueryPool.h"
#include "Core/Global/GlobalDefines.h"
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
    stltype::fixed_vector<QueryPoolVulkan, FRAMES_IN_FLIGHT, false> m_queryPools{FRAMES_IN_FLIGHT};
    f64 m_timestampPeriodNs{1.0};
    u32 m_maxPasses{0};
    u32 m_queryCount{0};
    stltype::fixed_vector<stltype::vector<u64>, FRAMES_IN_FLIGHT, false> m_timestampResults{FRAMES_IN_FLIGHT};
    stltype::fixed_vector<bool, FRAMES_IN_FLIGHT, false> m_poolInitialized;
    u32 m_currentFrameIdx{0};
};
