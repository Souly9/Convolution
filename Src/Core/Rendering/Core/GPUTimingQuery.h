#pragma once
#include "RenderTraitsMacros.h"
#include "Core/Global/GlobalDefines.h"
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

    virtual void ResetQueries(u32 frameIdx, CommandBuffer* pCmdBuffer = nullptr) = 0;
    virtual void ReadResults(u32 frameIdx) = 0;

    void SetCurrentFrameIdx(u32 frameIdx)
    {
        m_currentFrameIdx = frameIdx % SWAPCHAIN_IMAGES;
    }

    u32 RegisterPass(const stltype::string& name)
    {
        auto it = m_passNameToIndex.find(name);
        if (it != m_passNameToIndex.end())
            return it->second;

        u32 index = m_nextPassIndex++;
        m_passNameToIndex[name] = index;
        if (index >= m_results.size())
            m_results.resize(index + 1);
        m_results[index].passName = name;

        if (m_passRanThisFrame.size() < SWAPCHAIN_IMAGES)
            m_passRanThisFrame.resize(SWAPCHAIN_IMAGES);

        for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
            m_passRanThisFrame[i].resize(m_nextPassIndex, false);
        return index;
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

    void ClearRunFlags(u32 frameIdx)
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