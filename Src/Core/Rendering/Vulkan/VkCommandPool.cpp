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

void CommandPoolVulkan::ReturnBuffer(CBufferVulkan* buffer)
{
	m_availableBuffers.push(buffer);
}

bool CommandPoolVulkan::AreBuffersAvailable(size_t count) const
{
	return m_availableBuffers.size() >= count;
}

CBufferVulkan* CommandPoolVulkan::GetAvailableBuffer()
{
	auto pBuffer = m_availableBuffers.top();
	m_availableBuffers.pop();
	return pBuffer;
}

stltype::vector<CommandBuffer*> CommandPoolVulkan::GetAvailableBuffers(size_t count)
{
	stltype::vector<CommandBuffer*> rslt(count);
	for(size_t i = 0; i < count; ++i)
	{
		rslt.push_back(m_availableBuffers.top());
		m_availableBuffers.pop();
	}
	return rslt;
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
	m_commandBuffers.clear();
	
	VK_FREE_IF(m_commandPool, vkDestroyCommandPool(VkGlobals::GetLogicalDevice(), m_commandPool, VulkanAllocator()))
}

CBufferVulkan* CommandPoolVulkan::CreateCommandBuffer(const CommandBufferCreateInfo& createInfo)
{
	if (AreBuffersAvailable())
		return GetAvailableBuffer();

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_commandPool;
	allocInfo.level = createInfo.isPrimaryBuffer ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	allocInfo.commandBufferCount = 1;

	CommandBuffer* commandBuffer = &m_commandBuffers.emplace_back();
	DEBUG_ASSERT(vkAllocateCommandBuffers(VkGlobals::GetLogicalDevice(), &allocInfo, &commandBuffer->GetRef()) == VK_SUCCESS);

	commandBuffer->SetPool(this);

	return commandBuffer;
}

stltype::vector<CommandBuffer*> CommandPoolVulkan::CreateCommandBuffers(const CommandBufferCreateInfo& createInfo, const u32& count)
{
	if (AreBuffersAvailable())
		return GetAvailableBuffers(count);

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
