#include "VkGPUTimingQuery.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Vulkan/VkCommandBuffer.h"
#include "Core/Rendering/Vulkan/VkCommandPool.h"
#include "VkGlobals.h"

void GPUTimingQueryVulkan::Init(u32 maxPasses)
{
    m_maxPasses = maxPasses;
    m_queryCount = maxPasses * 2; // 2 timestamps per pass (start/end)

    const auto& props = VkGlobals::GetPhysicalDeviceProperties();
    m_timestampPeriodNs = static_cast<f64>(props.limits.timestampPeriod);

    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        m_queryPools[i][GRAPHICS_POOL_IDX].Init(VK_QUERY_TYPE_TIMESTAMP, m_queryCount);
        m_queryPools[i][COMPUTE_POOL_IDX].Init(VK_QUERY_TYPE_TIMESTAMP, m_queryCount);
        m_timestampResults[i][GRAPHICS_POOL_IDX].resize(m_queryCount, 0);
        m_timestampResults[i][COMPUTE_POOL_IDX].resize(m_queryCount, 0);
        vkResetQueryPool(VkGlobals::GetLogicalDevice(),
                         m_queryPools[i][GRAPHICS_POOL_IDX].GetRef(),
                         0,
                         m_queryCount);
        vkResetQueryPool(VkGlobals::GetLogicalDevice(),
                         m_queryPools[i][COMPUTE_POOL_IDX].GetRef(),
                         0,
                         m_queryCount);
    }

    m_results.reserve(maxPasses);
}

void GPUTimingQueryVulkan::Destroy()
{
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        m_queryPools[i][GRAPHICS_POOL_IDX].CleanUp();
        m_queryPools[i][COMPUTE_POOL_IDX].CleanUp();
        m_timestampResults[i][GRAPHICS_POOL_IDX].clear();
        m_timestampResults[i][COMPUTE_POOL_IDX].clear();
    }
    m_results.clear();
    m_passNameToIndex.clear();
    m_nextPassIndex = 0;
}

void GPUTimingQueryVulkan::ResetQueries(u32 frameIdx, CommandBuffer* pCmdBuffer)
{
    SetCurrentFrameIdx(frameIdx);
    ClearRunFlags(frameIdx);

    if (!m_enabled)
        return;

    m_poolInitialized[m_currentFrameIdx] = true;

    for (u32 poolIdx = 0; poolIdx < 2; ++poolIdx)
    {
        auto poolHandle = m_queryPools[m_currentFrameIdx][poolIdx].GetRef();
        if (poolHandle == VK_NULL_HANDLE)
            continue;

        if (pCmdBuffer)
        {
            pCmdBuffer->RecordCommand(ResetQueryPoolCmd(&m_queryPools[m_currentFrameIdx][poolIdx], 0, m_queryCount));
        }
        else
        {
            vkResetQueryPool(VkGlobals::GetLogicalDevice(), poolHandle, 0, m_queryCount);
        }
    }
}


void GPUTimingQueryVulkan::ReadResults(u32 frameIdx)
{
    u32 readFrameIdx = frameIdx % SWAPCHAIN_IMAGES;

    if (!m_enabled || !m_poolInitialized[readFrameIdx] || m_nextPassIndex == 0)
        return;

    bool anyPassRan = false;
    for (u32 i = 0; i < m_nextPassIndex && i < m_results.size(); ++i)
    {
        if (DidPassRun(i, frameIdx))
        {
            anyPassRan = true;
            break;
        }
    }
    if (!anyPassRan)
        return;

    for (u32 poolIdx = 0; poolIdx < 2; ++poolIdx)
    {
        auto poolHandle = m_queryPools[readFrameIdx][poolIdx].GetRef();
        if (poolHandle == VK_NULL_HANDLE)
            continue;

        vkGetQueryPoolResults(VkGlobals::GetLogicalDevice(),
                              poolHandle,
                              0,
                              m_queryCount,
                              m_queryCount * sizeof(u64),
                              m_timestampResults[readFrameIdx][poolIdx].data(),
                              sizeof(u64),
                              VK_QUERY_RESULT_64_BIT);
    }

    u64 globalMinTs = ~0ull;
    for (u32 i = 0; i < m_nextPassIndex && i < m_results.size(); ++i)
    {
        if (DidPassRun(i, frameIdx))
        {
            u32 queueFamily = m_results[i].queueFamilyIndex;
            u32 poolIdx = GRAPHICS_POOL_IDX;
            const auto& families = VkGlobals::GetQueueFamilyIndices();
            if (families.computeFamily.has_value() && queueFamily == families.computeFamily.value())
                poolIdx = COMPUTE_POOL_IDX;

            u64 startTs = m_timestampResults[readFrameIdx][poolIdx][i * 2];
            if (startTs != 0 && startTs < globalMinTs) globalMinTs = startTs;
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

        u32 queueFamily = m_results[i].queueFamilyIndex;
        u32 poolIdx = GRAPHICS_POOL_IDX;
        const auto& families = VkGlobals::GetQueueFamilyIndices();
        if (families.computeFamily.has_value() && queueFamily == families.computeFamily.value())
            poolIdx = COMPUTE_POOL_IDX;

        auto& results = m_timestampResults[readFrameIdx][poolIdx];
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

void GPUTimingQueryVulkan::WriteTimestampImpl(CommandBuffer* pCmdBuffer, u32 passIndex, bool isStart)
{
    u32 queryIndex = passIndex * 2 + (isStart ? 0 : 1);
    if (queryIndex >= m_queryCount)
        return;

    u32 poolIdx = GRAPHICS_POOL_IDX;
    auto* pVulkanCmd = static_cast<CBufferVulkan*>(pCmdBuffer);
    if (pVulkanCmd && pVulkanCmd->GetPool())
    {
        u32 queueFamily = pVulkanCmd->GetPool()->GetQueueFamilyIndex();
        if (isStart && passIndex < m_results.size())
            m_results[passIndex].queueFamilyIndex = queueFamily;

        const auto& families = VkGlobals::GetQueueFamilyIndices();
        if (families.computeFamily.has_value() && queueFamily == families.computeFamily.value())
            poolIdx = COMPUTE_POOL_IDX;
    }

    if (m_queryPools[m_currentFrameIdx][poolIdx].GetRef() == VK_NULL_HANDLE)
        return;

    WriteTimestampCmd cmd{};
    cmd.queryPool = &m_queryPools[m_currentFrameIdx][poolIdx];
    cmd.query = queryIndex;
    cmd.isStart = isStart;
    pCmdBuffer->RecordCommand(cmd);
}
