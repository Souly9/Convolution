#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/CommandPool.h"
#include "BackendDefines.h"

class CommandPoolVulkan : public TrackedResource
{
public:
	static CommandPoolVulkan Create(u32 graphicsFamilyIdx);

	~CommandPoolVulkan();
	virtual void CleanUp() override;

	CommandBuffer* CreateCommandBuffer(const CommandBufferCreateInfo& createInfo);
	stltype::vector<CommandBuffer*> CreateCommandBuffers(const CommandBufferCreateInfo& createInfo, const u32& count);

	VkCommandPool GetRef() const { return m_commandPool; }
protected:
	CommandPoolVulkan(u32 graphicsFamilyIdx);
	CommandPoolVulkan(u32 graphicsFamilyIdx, VkCommandPoolCreateFlagBits flags);

	VkCommandPool m_commandPool { VK_NULL_HANDLE };
	stltype::vector<CBufferVulkan> m_commandBuffers;
};

class TransferCommandPoolVulkan : public CommandPoolVulkan
{
public:
	static TransferCommandPoolVulkan Create();

protected:
	TransferCommandPoolVulkan(u32 transferFamilyIdx) : CommandPoolVulkan(transferFamilyIdx, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT) {}
};
