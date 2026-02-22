#pragma once
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
    virtual ~GPUTimingQueryBase() = default;

    virtual void Init(u32 maxPasses) = 0;
    virtual void Destroy() = 0;

    virtual void ResetQueries(u32 frameIdx) = 0;
    virtual void ReadResults(u32 frameIdx) = 0;

    void SetCurrentFrameIdx(u32 frameIdx)
    {
        m_currentFrameIdx = frameIdx % FRAMES_IN_FLIGHT;
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
        for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i)
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
        u32 f = frameIdx % FRAMES_IN_FLIGHT;
        for (auto& ran : m_passRanThisFrame[f])
            ran = false;
    }

    bool DidPassRun(u32 passIndex, u32 frameIdx) const
    {
        u32 f = frameIdx % FRAMES_IN_FLIGHT;
        return passIndex < m_passRanThisFrame[f].size() && m_passRanThisFrame[f][passIndex];
    }

    const stltype::vector<PassTimingResult>& GetResults() const
    {
        return m_results;
    }

    f32 GetTotalGPUTimeMs() const
    {
        // We run async compute so summing them will inflate our total time
        // Just take the max of the two
        f32 totalComputeTime = 0.f;
        f32 totalGraphicsTime = 0.f;
        for (const auto& r : m_results)
        {
            if (r.wasRun)
            {
                if (r.queueFamilyIndex == 0)
                    totalComputeTime += r.gpuTimeMs;
                else
                    totalGraphicsTime += r.gpuTimeMs;
            }
        }
        return stltype::max(totalComputeTime, totalGraphicsTime);
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
    stltype::array<stltype::vector<bool>, FRAMES_IN_FLIGHT> m_passRanThisFrame;
    stltype::hash_map<stltype::string, u32> m_passNameToIndex;
    u32 m_nextPassIndex{0};
    u32 m_currentFrameIdx{0};
    bool m_enabled{true};
};
