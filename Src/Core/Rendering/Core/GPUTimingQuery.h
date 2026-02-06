#pragma once
#include "Core/Global/GlobalDefines.h"
#include "RenderingForwardDecls.h"

struct PassTimingResult
{
    stltype::string passName;
    f32 gpuTimeMs{0.f};
    bool wasRun{false};
};

class GPUTimingQueryBase
{
public:
    virtual ~GPUTimingQueryBase() = default;

    virtual void Init(u32 maxPasses) = 0;
    virtual void Destroy() = 0;

    virtual void ResetQueries(CommandBuffer* pCmdBuffer, u32 frameIdx) = 0;
    virtual void ReadResults(u32 frameIdx) = 0;

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
        m_passRanThisFrame.resize(m_nextPassIndex, false);
        return index;
    }

    void WriteStartTimestamp(CommandBuffer* pCmdBuffer, u32 passIndex)
    {
        if (passIndex < m_passRanThisFrame.size())
            m_passRanThisFrame[passIndex] = true;
        WriteTimestampImpl(pCmdBuffer, passIndex, true);
    }

    void WriteEndTimestamp(CommandBuffer* pCmdBuffer, u32 passIndex)
    {
        WriteTimestampImpl(pCmdBuffer, passIndex, false);
    }

    void ClearRunFlags()
    {
        for (auto& ran : m_passRanThisFrame)
            ran = false;
    }

    bool DidPassRun(u32 passIndex) const
    {
        return passIndex < m_passRanThisFrame.size() && m_passRanThisFrame[passIndex];
    }

    const stltype::vector<PassTimingResult>& GetResults() const
    {
        return m_results;
    }

    f32 GetTotalGPUTimeMs() const
    {
        f32 total = 0.f;
        for (const auto& r : m_results)
        {
            if (r.wasRun)
                total += r.gpuTimeMs;
        }
        return total;
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
    stltype::vector<bool> m_passRanThisFrame;
    stltype::hash_map<stltype::string, u32> m_passNameToIndex;
    u32 m_nextPassIndex{0};
    bool m_enabled{true};
};
