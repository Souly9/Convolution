#pragma once
#include "Core/Rendering/Core/CommandPool.h"
#include "MtlCommandBuffer.h"
#include <EASTL/deque.h>

// Metal has no command pools; this just recycles CBufferMetal wrappers per queue
class CommandPoolMetal : public CommandPoolBase
{
public:
    CommandPoolMetal()
    {
    }
    static CommandPoolMetal Create(u32 queueFamilyIdx);

    ~CommandPoolMetal();

    CommandBuffer* CreateCommandBuffer(const CommandBufferCreateInfo& createInfo = CommandBufferCreateInfo{});
    stltype::vector<CommandBuffer*> CreateCommandBuffers(const CommandBufferCreateInfo& createInfo, const u32& count);

    bool IsValid() const
    {
        return m_isValid;
    }

    u32 GetQueueFamilyIndex() const
    {
        return m_queueFamilyIndex;
    }

    void ReturnCommandBuffer(CBufferMetal* commandBuffer);
    void ClearAll()
    {
        m_commandBuffers.clear();
        m_freeCommandBuffers.clear();
    }

protected:
    CommandPoolMetal(u32 queueFamilyIdx);

    stltype::deque<CommandBuffer> m_commandBuffers{};
    stltype::vector<CommandBuffer*> m_freeCommandBuffers{};
    u32 m_queueFamilyIndex{~0u};
    bool m_isValid{false};
};

