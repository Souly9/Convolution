#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/GPUTimingQuery.h"
#include "Core/Rendering/Core/CommandBuffer.h"

namespace Profiling
{
#if CONV_DEBUG
    static inline mathstl::Vector4 s_defaultProfilingColor{0.2f, 0.4f, 0.6f, 1.0f};
#endif

    inline void StartScope(CommandBuffer* pCmdBuffer, GPUTimingQueryBase* pTimingQuery, u32& timingIndex, const stltype::string& name, const mathstl::Vector4& color = {0.2f, 0.4f, 0.6f, 1.0f})
    {
#if CONV_DEBUG
        if (pTimingQuery && pTimingQuery->IsEnabled())
        {
            if (timingIndex == UINT32_MAX)
                timingIndex = pTimingQuery->RegisterPass(name);
            pTimingQuery->WriteStartTimestamp(pCmdBuffer, timingIndex);
        }
        pCmdBuffer->RecordCommand(StartProfilingScopeCmd{.name = name.c_str(), .color = color});
#endif
    }

    inline void EndScope(CommandBuffer* pCmdBuffer, GPUTimingQueryBase* pTimingQuery, u32 timingIndex)
    {
#if CONV_DEBUG
        if (pTimingQuery && pTimingQuery->IsEnabled() && timingIndex != UINT32_MAX)
            pTimingQuery->WriteEndTimestamp(pCmdBuffer, timingIndex);
        pCmdBuffer->RecordCommand(EndProfilingScopeCmd{});
#endif
    }
}
