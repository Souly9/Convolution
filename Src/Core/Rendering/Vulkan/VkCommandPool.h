#pragma once
#include "BackendDefines.h"
#include "Core/Rendering/Core/CommandPool.h"
#include "VkCommandBuffer.h"
#include <EASTL/deque.h>
#include <EASTL/stack.h>
class CommandPoolVulkan : public TrackedResource
{
public:
    CommandPoolVulkan()
    {
    }
    static CommandPoolVulkan Create(u32 graphicsFamilyIdx);

    ~CommandPoolVulkan();
    virtual void CleanUp() override;

    CommandBuffer* CreateCommandBuffer(const CommandBufferCreateInfo& createInfo = CommandBufferCreateInfo{});
    stltype::vector<CommandBuffer*> CreateCommandBuffers(const CommandBufferCreateInfo& createInfo, const u32& count);

    VkCommandPool GetRef() const
    {
        return m_commandPool;
    }

    bool IsValid() const
    {
        return m_commandPool != VK_NULL_HANDLE;
    }

    u32 GetQueueFamilyIndex() const
    {
        return m_queueFamilyIndex;
    }

    void ReturnCommandBuffer(const CommandBuffer* commandBuffer);
    void ClearAll()
    {
        m_commandBuffers.clear();
    }

    virtual void NamingCallBack(const stltype::string& name) override;

protected:
    CommandPoolVulkan(u32 graphicsFamilyIdx);
    CommandPoolVulkan(u32 graphicsFamilyIdx, VkCommandPoolCreateFlagBits flags);

    VkCommandPool m_commandPool{VK_NULL_HANDLE};
    stltype::deque<CBufferVulkan> m_commandBuffers{};
    u32 m_queueFamilyIndex{~0u};
};

class TransferCommandPoolVulkan : public CommandPoolVulkan
{
public:
    TransferCommandPoolVulkan()
    {
    }
    static TransferCommandPoolVulkan Create();

protected:
    TransferCommandPoolVulkan(u32 transferFamilyIdx)
        : CommandPoolVulkan(transferFamilyIdx, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
    {
    }
};
