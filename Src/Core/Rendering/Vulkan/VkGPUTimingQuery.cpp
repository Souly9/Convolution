#include "VkGPUTimingQuery.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Vulkan/VkCommandBuffer.h"
#include "Core/Rendering/Vulkan/VkCommandPool.h"
#include "VkGlobals.h"

void VkGPUTimingQuery::Init(u32 maxPasses)
{
    m_maxPasses = maxPasses;
    m_queryCount = maxPasses * 2; // 2 timestamps per pass (start/end)

    const auto& props = VkGlobals::GetPhysicalDeviceProperties();
    m_timestampPeriodNs = static_cast<f64>(props.limits.timestampPeriod);

    for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        m_queryPools[i].Init(VK_QUERY_TYPE_TIMESTAMP, m_queryCount);
        m_timestampResults[i].resize(m_queryCount, 0);
        m_poolInitialized.push_back(false);
    }

    m_results.reserve(maxPasses);
}

void VkGPUTimingQuery::Destroy()
{
    for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        m_queryPools[i].CleanUp();
        m_timestampResults[i].clear();
    }
    m_results.clear();
    m_passNameToIndex.clear();
    m_nextPassIndex = 0;
}

void VkGPUTimingQuery::ResetQueries(u32 frameIdx)
{
    SetCurrentFrameIdx(frameIdx);
    ClearRunFlags(frameIdx);

    if (!m_enabled || m_queryPools[m_currentFrameIdx].GetRef() == VK_NULL_HANDLE)
        return;

    m_poolInitialized[m_currentFrameIdx] = true;

    vkResetQueryPool(VkGlobals::GetLogicalDevice(), m_queryPools[m_currentFrameIdx].GetRef(), 0, m_queryCount);
}


void VkGPUTimingQuery::ReadResults(u32 frameIdx)
{
    u32 readFrameIdx = frameIdx % FRAMES_IN_FLIGHT;

    if (!m_enabled || !m_poolInitialized[readFrameIdx] || m_queryPools[readFrameIdx].GetRef() == VK_NULL_HANDLE || m_nextPassIndex == 0)
        return;

    for (u32 i = 0; i < m_nextPassIndex; ++i)
    {
        if (DidPassRun(i, frameIdx))
        {
            u32 queryIndex = i * 2;
            vkGetQueryPoolResults(VkGlobals::GetLogicalDevice(),
                                  m_queryPools[readFrameIdx].GetRef(),
                                  queryIndex,
                                  2,
                                  2 * sizeof(u64),
                                  &m_timestampResults[readFrameIdx][queryIndex],
                                  sizeof(u64),
                                  VK_QUERY_RESULT_64_BIT);
        }
    }

    auto& results = m_timestampResults[readFrameIdx];

    u64 globalMinTs = ~0ull;
    for (u32 i = 0; i < m_nextPassIndex && i < m_results.size(); ++i)
    {
        if (DidPassRun(i, frameIdx))
        {
            u64 startTs = results[i * 2];
            if (startTs < globalMinTs) globalMinTs = startTs;
        }
    }
    if (globalMinTs == ~0ull) globalMinTs = 0;

    f64 nsToMs = m_timestampPeriodNs / 1000000.0;

    for (u32 i = 0; i < m_nextPassIndex && i < m_results.size(); ++i)
    {
        m_results[i].wasRun = DidPassRun(i, frameIdx);

        if (!m_results[i].wasRun)
        {
            m_results[i].gpuTimeMs = 0.f;
            m_results[i].startMs = 0.f;
            m_results[i].endMs = 0.f;
            continue;
        }

        u64 startTs = results[i * 2];
        u64 endTs = results[i * 2 + 1];

        m_results[i].startMs = static_cast<f32>(static_cast<f64>(startTs - globalMinTs) * nsToMs);
        m_results[i].endMs = static_cast<f32>(static_cast<f64>(endTs - globalMinTs) * nsToMs);

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
    if (!m_enabled || m_queryPools[m_currentFrameIdx].GetRef() == VK_NULL_HANDLE)
        return;

    u32 queryIndex = passIndex * 2 + (isStart ? 0 : 1);
    if (queryIndex >= m_queryCount)
        return;

    if (isStart)
    {
        auto* pVulkanCmd = static_cast<CBufferVulkan*>(pCmdBuffer);
        if (pVulkanCmd && pVulkanCmd->GetPool())
        {
            if (passIndex < m_results.size())
                m_results[passIndex].queueFamilyIndex = pVulkanCmd->GetPool()->GetQueueFamilyIndex();
        }
    }

    WriteTimestampCmd cmd{};
    cmd.queryPool = &m_queryPools[m_currentFrameIdx];
    cmd.query = queryIndex;
    cmd.isStart = isStart;
    pCmdBuffer->RecordCommand(cmd);
}
