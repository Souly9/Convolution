#include "MtlCommandPool.h"

// TODO(Metal): hand out CBufferMetal backed by MTL::CommandQueue::commandBuffer()

CommandPoolMetal CommandPoolMetal::Create(u32 queueFamilyIdx)
{
    return CommandPoolMetal(queueFamilyIdx);
}

CommandPoolMetal::CommandPoolMetal(u32 queueFamilyIdx) : m_queueFamilyIndex(queueFamilyIdx), m_isValid(true)
{
}

CommandPoolMetal::~CommandPoolMetal()
{
    TRACKED_DESC_IMPL
}

CommandBuffer* CommandPoolMetal::CreateCommandBuffer(const CommandBufferCreateInfo& createInfo)
{
    return nullptr;
}

stltype::vector<CommandBuffer*> CommandPoolMetal::CreateCommandBuffers(const CommandBufferCreateInfo& createInfo,
                                                                       const u32& count)
{
    return {};
}

void CommandPoolMetal::ReturnCommandBuffer(CBufferMetal* commandBuffer)
{
}
