#include "Core/Global/GlobalDefines.h"
#include "VkCommandPool.h"
#include "VkGlobals.h"

void CommandPoolVulkan::ReturnCommandBuffer(CommandBuffer* pBuffer)
{
	if(pBuffer == nullptr) return;
	m_commandBuffers.erase(stltype::remove_if(m_commandBuffers.begin(), m_commandBuffers.end(), 
		[pBuffer](CommandBuffer& buffer) { return buffer.GetRef() == pBuffer->GetRef(); }),
		m_commandBuffers.end());
}

CommandPoolVulkan::CommandPoolVulkan(u32 graphicsFamilyIdx) : CommandPoolVulkan(graphicsFamilyIdx, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
{
}

CommandPoolVulkan::CommandPoolVulkan(u32 graphicsFamilyIdx, VkCommandPoolCreateFlagBits flags) : CommandPoolVulkan()
{
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = flags;
	poolInfo.queueFamilyIndex = graphicsFamilyIdx;

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
	VK_FREE_IF(m_commandPool, vkDestroyCommandPool(VkGlobals::GetLogicalDevice(), m_commandPool, VulkanAllocator()))
}

CBufferVulkan* CommandPoolVulkan::CreateCommandBuffer(const CommandBufferCreateInfo& createInfo)
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool;
	allocInfo.level = createInfo.isPrimaryBuffer ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	allocInfo.commandBufferCount = 1;
	m_commandBuffers.reserve(128);

	VkCommandBuffer buffer;
	DEBUG_ASSERT(vkAllocateCommandBuffers(VkGlobals::GetLogicalDevice(), &allocInfo, &buffer) == VK_SUCCESS);
	CommandBuffer* commandBuffer = &m_commandBuffers.emplace_back(buffer);

	commandBuffer->SetPool(this);

	return commandBuffer;
}

stltype::vector<CommandBuffer*> CommandPoolVulkan::CreateCommandBuffers(const CommandBufferCreateInfo& createInfo, const u32& count)
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool;
	allocInfo.level = createInfo.isPrimaryBuffer ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	allocInfo.commandBufferCount = count;
	m_commandBuffers.reserve(128);

	stltype::vector<VkCommandBuffer> buffers;
	buffers.resize(count);
	stltype::vector<CommandBuffer*> rsltBuffers;
	rsltBuffers.reserve(count);

	DEBUG_ASSERT(vkAllocateCommandBuffers(VkGlobals::GetLogicalDevice(), &allocInfo, buffers.data()) == VK_SUCCESS);

	// becomes nullptr when vector resizes
	for (auto& buffer : buffers)
	{
		auto* pBuffer = &m_commandBuffers.emplace_back(buffer);
		pBuffer->SetPool(this);
		rsltBuffers.emplace_back(pBuffer);
	}

	return rsltBuffers;
}

TransferCommandPoolVulkan TransferCommandPoolVulkan::Create()
{
	return { VkGlobals::GetQueueFamilyIndices().transferFamily.value() };
}
