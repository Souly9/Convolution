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
}
