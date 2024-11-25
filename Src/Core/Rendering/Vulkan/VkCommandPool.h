#pragma once
#include <EASTL/stack.h>
#include "BackendDefines.h"
#include "Core/Rendering/Core/CommandPool.h"

class CommandPoolVulkan : public TrackedResource
{
public:
	CommandPoolVulkan() {}
	static CommandPoolVulkan Create(u32 graphicsFamilyIdx);

	~CommandPoolVulkan();
	virtual void CleanUp() override;

	CommandBuffer* CreateCommandBuffer(const CommandBufferCreateInfo& createInfo = CommandBufferCreateInfo{});
	stltype::vector<CommandBuffer*> CreateCommandBuffers(const CommandBufferCreateInfo& createInfo, const u32& count);

	VkCommandPool GetRef() const { return m_commandPool; }

	bool IsValid() const { return m_commandPool != VK_NULL_HANDLE; }

	void ReturnCommandBuffer(CommandBuffer* commandBuffer);
protected:
	CommandPoolVulkan(u32 graphicsFamilyIdx);
	CommandPoolVulkan(u32 graphicsFamilyIdx, VkCommandPoolCreateFlagBits flags);

	VkCommandPool m_commandPool { VK_NULL_HANDLE };
	stltype::vector<CBufferVulkan> m_commandBuffers;
};

class TransferCommandPoolVulkan : public CommandPoolVulkan
{
public:
	TransferCommandPoolVulkan() {}
	static TransferCommandPoolVulkan Create();

protected:
	TransferCommandPoolVulkan(u32 transferFamilyIdx) : CommandPoolVulkan(transferFamilyIdx, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT) {}
};
