#include "VkQueryPool.h"
#include "VkGlobals.h"

QueryPoolVulkan::~QueryPoolVulkan()
{
    TRACKED_DESC_IMPL
}

void QueryPoolVulkan::CleanUp()
{
    VK_FREE_IF(m_pool, vkDestroyQueryPool(VK_LOGICAL_DEVICE, m_pool, VulkanAllocator()));
}

void QueryPoolVulkan::Init(VkQueryType type, u32 count, VkQueryPipelineStatisticFlags stats)
{
    VkQueryPoolCreateInfo queryPoolInfo{};
    queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryPoolInfo.queryType = type;
    queryPoolInfo.queryCount = count;
    queryPoolInfo.pipelineStatistics = stats;

    vkCreateQueryPool(VK_LOGICAL_DEVICE, &queryPoolInfo, nullptr, &m_pool);
    m_type = type;
    m_count = count;
    m_statsFlags = stats;
}

void QueryPoolVulkan::GetPipelineStatistics(u32 index, u32 count, stltype::vector<u64>& results)
{
    if (m_type != VK_QUERY_TYPE_PIPELINE_STATISTICS)
    {
        return;
    }

    // Calculate number of stats enabled
    u32 numStats = 0;
    u32 flags = m_statsFlags;
    while (flags > 0)
    {
        if (flags & 1)
        {
            numStats++;
        }
        flags >>= 1;
    }

    if (numStats == 0)
        return;

    results.resize(count * numStats);
    DEBUG_ASSERT(vkGetQueryPoolResults(VK_LOGICAL_DEVICE,
                          m_pool,
                          index,
                          count,
                          results.size() * sizeof(u64),
                          results.data(),
                          numStats * sizeof(u64),
                          VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) == VK_SUCCESS);
}
