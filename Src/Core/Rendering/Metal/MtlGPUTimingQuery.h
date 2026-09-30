#pragma once
#include "Core/Rendering/Core/GPUTimingQuery.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Global/GlobalDefines.h"
#include "MtlQueryPool.h"

// Apple GPUs only sample timestamps at stage/encoder boundaries (MTL::CounterSamplingPoint)
class GPUTimingQueryMetal : public GPUTimingQueryBase
{
public:
    void Init(u32 maxPasses) override;
    void Destroy() override;

    void ResetQueries(u32 frameIdx, CommandBuffer* pGraphicsCmdBuffer = nullptr, CommandBuffer* pComputeCmdBuffer = nullptr) override;
    void ResetQueriesForQueue(u32 frameIdx, CommandBuffer* pCmdBuffer, QueueType queueType) override;
    void ResetQueriesHost(u32 frameIdx) override;
    void ReadResults(u32 frameIdx) override;

protected:
    void WriteTimestampImpl(CommandBuffer* pCmdBuffer, u32 passIndex, bool isStart) override;

private:
    stltype::array<QueryPoolMetal, SWAPCHAIN_IMAGES> m_queryPools{};
    u32 m_maxPasses{0};
};
