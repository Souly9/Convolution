#include "MtlGPUTimingQuery.h"

// TODO(Metal): MTL::CounterSampleBuffer with the timestamp counter set

void GPUTimingQueryMetal::Init(u32 maxPasses)
{
    m_maxPasses = maxPasses;
}

void GPUTimingQueryMetal::Destroy()
{
}

void GPUTimingQueryMetal::ResetQueries(u32 frameIdx, CommandBuffer* pGraphicsCmdBuffer, CommandBuffer* pComputeCmdBuffer)
{
}

void GPUTimingQueryMetal::ResetQueriesForQueue(u32 frameIdx, CommandBuffer* pCmdBuffer, QueueType queueType)
{
}

void GPUTimingQueryMetal::ResetQueriesHost(u32 frameIdx)
{
}

void GPUTimingQueryMetal::ReadResults(u32 frameIdx)
{
}

void GPUTimingQueryMetal::WriteTimestampImpl(CommandBuffer* pCmdBuffer, u32 passIndex, bool isStart)
{
}
