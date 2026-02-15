#pragma once
#include "Core/Rendering/Core/Resource.h"

class QueryPoolVulkan : public TrackedResource
{
public:
    QueryPoolVulkan() = default;
    virtual ~QueryPoolVulkan();

    virtual void CleanUp() override;
    void Init(VkQueryType type, u32 count, VkQueryPipelineStatisticFlags stats = 0);

    VkQueryPool GetRef() const
    {
        return m_pool;
    }

    // Accessors
    VkQueryType GetType() const
    {
        return m_type;
    }
    u32 GetCount() const
    {
        return m_count;
    }

    void GetPipelineStatistics(u32 index, u32 count, stltype::vector<u64>& results);

private:
    VkQueryPool m_pool{VK_NULL_HANDLE};
    VkQueryType m_type{VK_QUERY_TYPE_MAX_ENUM};
    u32 m_count{0};
    VkQueryPipelineStatisticFlags m_statsFlags{0};
};
