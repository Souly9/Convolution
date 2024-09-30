#include "VkCommandPool.h"
#include "VkGlobals.h"

CommandPoolVulkan::CommandPoolVulkan(u32 graphicsFamilyIdx) : CommandPoolVulkan(graphicsFamilyIdx, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
{
}

CommandPoolVulkan::CommandPoolVulkan(u32 graphicsFamilyIdx, VkCommandPoolCreateFlagBits flags)
{
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = flags;
	poolInfo.queueFamilyIndex = graphicsFamilyIdx;
	m_commandBuffers.reserve(1024);

	DEBUG_ASSERT(vkCreateCommandPool(VkGlobals::GetLogicalDevice(), &poolInfo, VulkanAllocator(), &m_commandPool) == VK_SUCCESS);
}

CommandPoolVulkan CommandPoolVulkan::Create(u32 graphicsFamilyIdx)
{
	return { graphicsFamilyIdx };
}

CommandPoolVulkan::~CommandPoolVulkan()
{
	TRACKED_DESC_IMPL
}

void CommandPoolVulkan::CleanUp()
{
	for (auto& commandBuffer : m_commandBuffers)
	{
		commandBuffer.Destroy(GetRef());
	}
	m_commandBuffers.clear();
	VK_FREE_IF(m_commandPool, vkDestroyCommandPool(VkGlobals::GetLogicalDevice(), m_commandPool, VulkanAllocator()))
}

CBufferVulkan* CommandPoolVulkan::CreateCommandBuffer(const CommandBufferCreateInfo& createInfo)
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool;
	allocInfo.level = createInfo.isPrimaryBuffer ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	allocInfo.commandBufferCount = 1;

	CommandBuffer* commandBuffer = &m_commandBuffers.emplace_back();
	DEBUG_ASSERT(vkAllocateCommandBuffers(VkGlobals::GetLogicalDevice(), &allocInfo, &commandBuffer->GetRef()) == VK_SUCCESS);

	return commandBuffer;
}

stltype::vector<CommandBuffer*> CommandPoolVulkan::CreateCommandBuffers(const CommandBufferCreateInfo& createInfo, const u32& count)
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool;
	allocInfo.level = createInfo.isPrimaryBuffer ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	allocInfo.commandBufferCount = count;

	stltype::vector<VkCommandBuffer> buffers;
	buffers.resize(count);
	stltype::vector<CommandBuffer*> rsltBuffers;
	rsltBuffers.reserve(count);

	DEBUG_ASSERT(vkAllocateCommandBuffers(VkGlobals::GetLogicalDevice(), &allocInfo, buffers.data()) == VK_SUCCESS);

	// becomes nullptr when vector resizes
	for (auto& buffer : buffers)
	{
		rsltBuffers.emplace_back(&m_commandBuffers.emplace_back(buffer));
	}

	return rsltBuffers;
}

TransferCommandPoolVulkan TransferCommandPoolVulkan::Create()
{
	return { VkGlobals::GetQueueFamilyIndices().transferFamily.value() };
}
