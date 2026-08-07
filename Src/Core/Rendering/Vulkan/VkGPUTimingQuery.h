#pragma once
#include "Core/Rendering/Core/GPUTimingQuery.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Rendering/Vulkan/VkQueryPool.h"
#include "Core/Global/GlobalDefines.h"
#include <vulkan/vulkan.h>

class GPUTimingQueryVulkan : public GPUTimingQueryBase
{
public:
    void Init(u32 maxPasses) override;
    void Destroy() override;

    void ResetQueries(u32 frameIdx, CommandBuffer* pGraphicsCmdBuffer = nullptr, CommandBuffer* pComputeCmdBuffer = nullptr) override;
    void ResetQueriesForQueue(u32 frameIdx, CommandBuffer* pCmdBuffer, QueueType queueType) override;
    void ReadResults(u32 frameIdx) override;
    void ClearRunFlags(u32 frameIdx) override;

protected:
    void WriteTimestampImpl(CommandBuffer* pCmdBuffer, u32 passIndex, bool isStart) override;

private:
    u32 GetPoolIndex(u32 queueFamilyIndex) const;

    stltype::array<stltype::array<QueryPoolVulkan, 2>, SWAPCHAIN_IMAGES> m_queryPools{};
    f64 m_timestampPeriodNs{1.0};
    u32 m_maxPasses{0};
    u32 m_queryCount{0};
    // 0 = Graphics, 1 = Compute
    static constexpr u32 GRAPHICS_POOL_IDX = 0;
    static constexpr u32 COMPUTE_POOL_IDX = 1;

    stltype::array<stltype::array<stltype::vector<u64>, 2>, SWAPCHAIN_IMAGES> m_timestampResults{};
    stltype::array<bool, SWAPCHAIN_IMAGES> m_poolInitialized{};
    stltype::array<stltype::array<bool, 2>, SWAPCHAIN_IMAGES> m_poolResetThisFrame{};
};
