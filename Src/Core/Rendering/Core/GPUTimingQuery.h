#pragma once
#include "RenderTraitsMacros.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "RenderingForwardDecls.h"

struct PassTimingResult
{
    stltype::string passName;
    f32 gpuTimeMs{0.f};
    f32 startMs{0.f};
    f32 endMs{0.f};
    u32 queueFamilyIndex{0};
    bool wasRun{false};
};

class GPUTimingQueryBase
{
public:
    GPUTimingQueryBase()
    {
        m_passRanThisFrame.resize(SWAPCHAIN_IMAGES);
    }
    virtual ~GPUTimingQueryBase() = default;

    virtual void Init(u32 maxPasses) = 0;
    virtual void Destroy() = 0;

    virtual void ResetQueries(u32 frameIdx, CommandBuffer* pGraphicsCmdBuffer = nullptr, CommandBuffer* pComputeCmdBuffer = nullptr) = 0;
    virtual void ResetQueriesForQueue(u32 frameIdx, CommandBuffer* pCmdBuffer, QueueType queueType) = 0;
    virtual void ResetQueriesHost(u32 frameIdx) = 0;
    virtual void ReadResults(u32 frameIdx) = 0;

    void SetCurrentFrameIdx(u32 frameIdx)
    {
        u32 newIdx = frameIdx % SWAPCHAIN_IMAGES;
        if (m_currentFrameIdx != newIdx)
        {
            m_currentFrameIdx = newIdx;
            ClearRunFlags(m_currentFrameIdx);
        }
    }

    u32 GetQueryOffset(u32 passIndex) const
    {
        if (passIndex < m_passQueryOffset.size())
            return m_passQueryOffset[passIndex];
        return passIndex * 2;
    }

    u32 GetViewCount(u32 passIndex) const
    {
        if (passIndex < m_passViewCount.size())
            return m_passViewCount[passIndex];
        return 1;
    }

    u32 RegisterPass(const stltype::string& name, u32 viewMask = 1)
    {
        u32 count = 1;
        if (viewMask > 0)
        {
            u32 c = 0;
            u32 m = viewMask;
            while (m > 0) { c += (m & 1u); m >>= 1u; }
            count = c > 0 ? c : 1;
        }

        auto it = m_passNameToIndex.find(name);
        if (it != m_passNameToIndex.end())
        {
            u32 idx = it->second;
            if (idx < m_passViewCount.size() && m_passViewCount[idx] != count)
            {
                m_passViewCount[idx] = count;
                RecomputeQueryOffsets();
            }
            return idx;
        }

        u32 index = m_nextPassIndex++;
        m_passNameToIndex[name] = index;
        if (index >= m_results.size())
            m_results.resize(index + 1);
        m_results[index].passName = name;

        if (index >= m_passViewCount.size())
        {
            m_passViewCount.resize(index + 1, 1);
            m_passQueryOffset.resize(index + 1, 0);
        }
        m_passViewCount[index] = count;
        RecomputeQueryOffsets();

        if (m_passRanThisFrame.size() < SWAPCHAIN_IMAGES)
            m_passRanThisFrame.resize(SWAPCHAIN_IMAGES);

        for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
            m_passRanThisFrame[i].resize(m_nextPassIndex, false);
        return index;
    }

    void RecomputeQueryOffsets()
    {
        u32 currentOffset = 0;
        m_passQueryOffset.resize(m_nextPassIndex);
        for (u32 i = 0; i < m_nextPassIndex; ++i)
        {
            m_passQueryOffset[i] = currentOffset;
            u32 viewCount = (i < m_passViewCount.size()) ? m_passViewCount[i] : 1;
            currentOffset += (2 * viewCount);
        }
        m_totalQuerySlotsUsed = currentOffset;
    }

    void WriteStartTimestamp(CommandBuffer* pCmdBuffer, u32 passIndex)
    {
        if (passIndex < m_passRanThisFrame[m_currentFrameIdx].size())
            m_passRanThisFrame[m_currentFrameIdx][passIndex] = true;
        WriteTimestampImpl(pCmdBuffer, passIndex, true);
    }

    void WriteEndTimestamp(CommandBuffer* pCmdBuffer, u32 passIndex)
    {
        WriteTimestampImpl(pCmdBuffer, passIndex, false);
    }

    virtual void ClearRunFlags(u32 frameIdx)
    {
        u32 f = frameIdx % SWAPCHAIN_IMAGES;
        for (auto& ran : m_passRanThisFrame[f])
            ran = false;
    }

    bool DidPassRun(u32 passIndex, u32 frameIdx) const
    {
        u32 f = frameIdx % SWAPCHAIN_IMAGES;
        return passIndex < m_passRanThisFrame[f].size() && m_passRanThisFrame[f][passIndex];
    }

    const stltype::vector<PassTimingResult>& GetResults() const
    {
        return m_results;
    }

    f32 GetTotalGPUTimeMs() const
    {
        f32 minStart = 1e10f;
        f32 maxEnd = 0.f;
        bool anyRun = false;

        for (const auto& r : m_results)
        {
            if (r.wasRun)
            {
                minStart = stltype::min(minStart, r.startMs);
                maxEnd = stltype::max(maxEnd, r.endMs);
                anyRun = true;
            }
        }
        return anyRun ? (maxEnd - minStart) : 0.f;
    }

    bool IsEnabled() const
    {
        return m_enabled;
    }

    void SetEnabled(bool enabled)
    {
        m_enabled = enabled;
    }

protected:
    virtual void WriteTimestampImpl(CommandBuffer* pCmdBuffer, u32 passIndex, bool isStart) = 0;

    stltype::vector<PassTimingResult> m_results;
    stltype::vector<u32> m_passViewCount;
    stltype::vector<u32> m_passQueryOffset;
    u32 m_totalQuerySlotsUsed{0};
    stltype::fixed_vector<stltype::vector<bool>, SWAPCHAIN_IMAGES> m_passRanThisFrame;
    stltype::hash_map<stltype::string, u32> m_passNameToIndex;
    u32 m_nextPassIndex{0};
    u32 m_currentFrameIdx{0};
    bool m_enabled{true};
};

#include "Core/Rendering/Core/APITraits.h"
#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkGPUTimingQuery.h"
#include "Core/Rendering/Vulkan/VulkanTraits.h"

#endif

template <typename API>
class GPUTimingQueryT : public APITraits<API>::GPUTimingQueryType
{
public:
    using APITraits<API>::GPUTimingQueryType::GPUTimingQueryType;
    DECLARE_RENDER_RESOURCE_TRAITS(GPUTimingQueryT, GPUTimingQueryType)
};