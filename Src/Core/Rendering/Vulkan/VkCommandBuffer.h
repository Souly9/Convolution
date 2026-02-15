#pragma once
#include "BackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/Synchronization.h"

class CBufferVulkan : public CBuffer
{
public:
    CBufferVulkan() = default;
    CBufferVulkan(VkCommandBuffer commandBuffer);
    ~CBufferVulkan();

    void Bake();
    
    CommandBufferStats& GetStats()
    {
        return m_stats;
    }

    const VkCommandBuffer& GetRef() const
    {
        return m_commandBuffer;
    }
    VkCommandBuffer& GetRef()
    {
        return m_commandBuffer;
    }
    void SetPool(CommandPoolVulkan* pool)
    {
        m_pool = pool;
    }
    CommandPoolVulkan* GetPool() const
    {
        return m_pool;
    }

    void BeginBuffer();
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

    void AddWaitSemaphores(const stltype::vector<Semaphore*>& semaphores)
    {
        for (auto sem : semaphores)
        {
            AddWaitSemaphore(sem);
        }
    }

    void AddSignalSemaphores(const stltype::vector<Semaphore*>& semaphores)
    {
        for (auto sem : semaphores)
        {
            AddSignalSemaphore(sem);
        }
    }

    void AddWaitSemaphore(Semaphore* pSemaphore);
    void AddSignalSemaphore(Semaphore* pSemaphore);

    // Timeline semaphore support
    void SetTimelineWait(TimelineSemaphore* pSemaphore, u64 waitValue);
    void SetTimelineSignal(TimelineSemaphore* pSemaphore, u64 signalValue);

    bool HasTimelineSemaphores() const
    {
        return m_timelineWaitSemaphore != VK_NULL_HANDLE || m_timelineSignalSemaphore != VK_NULL_HANDLE;
    }

    VkSemaphore GetTimelineWaitSemaphore() const
    {
        return m_timelineWaitSemaphore;
    }
    VkSemaphore GetTimelineSignalSemaphore() const
    {
        return m_timelineSignalSemaphore;
    }
    u64 GetTimelineWaitValue() const
    {
        return m_timelineWaitValue;
    }
    u64 GetTimelineSignalValue() const
    {
        return m_timelineSignalValue;
    }

    void ClearSemaphores()
    {
        m_waitSemaphores.clear();
        m_signalSemaphores.clear();
        m_timelineWaitSemaphore = VK_NULL_HANDLE;
        m_timelineSignalSemaphore = VK_NULL_HANDLE;
        m_timelineWaitValue = 0;
        m_timelineSignalValue = 0;
    }

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

    virtual void NamingCallBack(const stltype::string& name) override;

protected:
    // Optional semaphores to wait on and signal, used when submitting the command buffer
    stltype::vector<RawSemaphoreHandle> m_waitSemaphores;
    stltype::vector<RawSemaphoreHandle> m_signalSemaphores;

    // Timeline semaphores (only one pair per command buffer)
    VkSemaphore m_timelineWaitSemaphore{VK_NULL_HANDLE};
    VkSemaphore m_timelineSignalSemaphore{VK_NULL_HANDLE};
    u64 m_timelineWaitValue{0};
    u64 m_timelineSignalValue{0};

    CommandPoolVulkan* m_pool{nullptr};
    VkCommandBuffer m_commandBuffer{VK_NULL_HANDLE};
    u32 m_waitStages;
    u32 m_signalStages;
};