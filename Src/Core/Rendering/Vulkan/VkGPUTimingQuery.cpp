#include "VkGPUTimingQuery.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "VkGlobals.h"

void VkGPUTimingQuery::Init(u32 maxPasses)
{
    m_maxPasses = maxPasses;
    m_queryCount = maxPasses * 2; // 2 timestamps per pass (start/end)

    const auto& props = VkGlobals::GetPhysicalDeviceProperties();
    m_timestampPeriodNs = static_cast<f64>(props.limits.timestampPeriod);

    m_queryPool.Init(VK_QUERY_TYPE_TIMESTAMP, m_queryCount);

    m_timestampResults.resize(m_queryCount, 0);
    m_results.reserve(maxPasses);
}

void VkGPUTimingQuery::Destroy()
{
    m_queryPool.CleanUp();
    m_timestampResults.clear();
    m_results.clear();
    m_passNameToIndex.clear();
    m_nextPassIndex = 0;
}

void VkGPUTimingQuery::ResetQueries(CommandBuffer* pCmdBuffer, u32 frameIdx)
{
    if (!m_enabled || m_queryPool.GetRef() == VK_NULL_HANDLE)
        return;

    // Note: Don't clear run flags here - we need them for ReadResults which happens after this

    ResetQueryPoolCmd cmd{};
    cmd.queryPool = &m_queryPool;
    cmd.firstQuery = 0;
    cmd.queryCount = m_queryCount;
    pCmdBuffer->RecordCommand(cmd);
}

void VkGPUTimingQuery::ReadResults(u32 frameIdx)
{
    if (!m_enabled || m_queryPool.GetRef() == VK_NULL_HANDLE || m_nextPassIndex == 0)
        return;

    u32 queryCount = m_nextPassIndex * 2;

    VkResult result = vkGetQueryPoolResults(VkGlobals::GetLogicalDevice(),
                                            m_queryPool.GetRef(),
                                            0,
                                            queryCount,
                                            queryCount * sizeof(u64),
                                            m_timestampResults.data(),
                                            sizeof(u64),
                                            VK_QUERY_RESULT_64_BIT);

    if (result != VK_SUCCESS && result != VK_NOT_READY)
        return;

    // Convert timestamps to milliseconds, set wasRun flag
    for (u32 i = 0; i < m_nextPassIndex && i < m_results.size(); ++i)
    {
        m_results[i].wasRun = DidPassRun(i);

        if (!m_results[i].wasRun)
        {
            m_results[i].gpuTimeMs = 0.f;
            continue;
        }

        u64 startTs = m_timestampResults[i * 2];
        u64 endTs = m_timestampResults[i * 2 + 1];

        if (endTs > startTs)
        {
            f64 deltaNs = static_cast<f64>(endTs - startTs) * m_timestampPeriodNs;
            m_results[i].gpuTimeMs = static_cast<f32>(deltaNs / 1000000.0);
        }
        else
        {
            m_results[i].gpuTimeMs = 0.f;
        }
    }
}

void VkGPUTimingQuery::WriteTimestampImpl(CommandBuffer* pCmdBuffer, u32 passIndex, bool isStart)
{
    if (!m_enabled || m_queryPool.GetRef() == VK_NULL_HANDLE)
        return;

    u32 queryIndex = passIndex * 2 + (isStart ? 0 : 1);
    if (queryIndex >= m_queryCount)
        return;

    WriteTimestampCmd cmd{};
    cmd.queryPool = &m_queryPool;
    cmd.query = queryIndex;
    cmd.isStart = isStart;
    pCmdBuffer->RecordCommand(cmd);
}
