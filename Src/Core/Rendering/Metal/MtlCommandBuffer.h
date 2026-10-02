#pragma once
#include "MtlBackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Metal/MetalTraits.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/Synchronization.h"

class CommandPoolMetal;

// Replays the recorded Command list into typed Metal encoders (render/compute/blit/AS) at Bake()
class CBufferMetal : public CommandBufferBase
{
public:
    CBufferMetal() = default;
    ~CBufferMetal();

    void Bake();


    MTL::CommandBuffer* GetRef() const
    {
        return m_commandBuffer;
    }
    void SetPool(CommandPoolMetal* pool)
    {
        m_pool = pool;
    }
    CommandPoolMetal* GetPool() const
    {
        return m_pool;
    }

    void BeginBufferForSingleSubmit();
    void BeginRendering(BeginRenderingCmd& cmd);
    void BeginRendering(BeginRenderingBaseCmd& cmd);
    void EndRendering();
    void EndBuffer();

    void ResetBuffer();

    void Destroy();

    const stltype::vector<RawSemaphoreHandle>& GetWaitSemaphores() const
    {
        return m_waitSemaphores;
    }
    const stltype::vector<RawSemaphoreHandle>& GetSignalSemaphores() const
    {
        return m_signalSemaphores;
    }

    // Encoded as encodeWait/encodeSignalEvent on the MTL::CommandBuffer
    void AddWaitSemaphore(Semaphore* pSemaphore);
    void AddSignalSemaphore(Semaphore* pSemaphore);

    void AddTimelineWait(TimelineSemaphore* pSemaphore, u64 waitValue);
    void AddTimelineSignal(TimelineSemaphore* pSemaphore, u64 signalValue);

    struct TimelineSemaphoreInfo
    {
        MTL::SharedEvent* event;
        u64 value;
    };

    const stltype::vector<TimelineSemaphoreInfo>& GetTimelineWaits() const
    {
        return m_timelineWaits;
    }
    const stltype::vector<TimelineSemaphoreInfo>& GetTimelineSignals() const
    {
        return m_timelineSignals;
    }

    void ClearSemaphores()
    {
        m_waitSemaphores.clear();
        m_signalSemaphores.clear();
        m_timelineWaits.clear();
        m_timelineSignals.clear();
    }

    // Stage masks have no Metal equivalent at submit level; kept for interface parity
    void SetWaitStages(SyncStages stages);
    void SetSignalStages(SyncStages stages);

    u32 GetWaitStages() const
    {
        return m_waitStages;
    }
    u32 GetSignalStages() const
    {
        return m_signalStages;
    }

protected:
    stltype::vector<RawSemaphoreHandle> m_waitSemaphores;
    stltype::vector<RawSemaphoreHandle> m_signalSemaphores;

    stltype::vector<TimelineSemaphoreInfo> m_timelineWaits;
    stltype::vector<TimelineSemaphoreInfo> m_timelineSignals;

    CommandPoolMetal* m_pool{nullptr};
    MTL::CommandBuffer* m_commandBuffer{nullptr};
    u32 m_waitStages{0};
    u32 m_signalStages{0};
};
