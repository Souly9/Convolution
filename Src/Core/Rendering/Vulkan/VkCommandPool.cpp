#include "VkCommandPool.h"
#include "Core/Global/GlobalDefines.h"
#include "VkGlobals.h"

#define MAX_REASONABLE_COMMAND_BUFFERS 2048

void CommandPoolVulkan::ReturnCommandBuffer(CommandBuffer* pBuffer)
{
    if (pBuffer == nullptr)
        return;
    m_freeCommandBuffers.push_back(pBuffer);
}

void CommandPoolVulkan::NamingCallBack(const stltype::string& name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo{};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_COMMAND_POOL;
    nameInfo.objectHandle = (u64)GetRef();
    nameInfo.pObjectName = name.c_str();

    vkSetDebugUtilsObjectName(VkGlobals::GetLogicalDevice(), &nameInfo);
}
CommandPoolVulkan::CommandPoolVulkan(u32 graphicsFamilyIdx)
    : CommandPoolVulkan(graphicsFamilyIdx, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
{
}

CommandPoolVulkan::CommandPoolVulkan(u32 graphicsFamilyIdx, VkCommandPoolCreateFlagBits flags) : CommandPoolVulkan()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = flags;
    poolInfo.queueFamilyIndex = graphicsFamilyIdx;
    m_queueFamilyIndex = graphicsFamilyIdx;

    DEBUG_ASSERT(vkCreateCommandPool(VkGlobals::GetLogicalDevice(), &poolInfo, VulkanAllocator(), &m_commandPool) ==
                 VK_SUCCESS);
}

CommandPoolVulkan CommandPoolVulkan::Create(u32 graphicsFamilyIdx)
{
    return {graphicsFamilyIdx};
}

CommandPoolVulkan::~CommandPoolVulkan()
{
    TRACKED_DESC_IMPL
}

void CommandPoolVulkan::CleanUp(){
    VK_FREE_IF(m_commandPool, vkDestroyCommandPool(VkGlobals::GetLogicalDevice(), m_commandPool, VulkanAllocator()))}

CBufferVulkan* CommandPoolVulkan::CreateCommandBuffer(const CommandBufferCreateInfo& createInfo)
{
    ScopedZone("CommandPoolVulkan::CreateCommandBuffer");

    if (!m_freeCommandBuffers.empty())
    {
        CommandBuffer* pBuf = m_freeCommandBuffers.back();
        m_freeCommandBuffers.pop_back();
        pBuf->ResetBuffer();
        return pBuf;
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = createInfo.isPrimaryBuffer ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    allocInfo.commandBufferCount = 1;

    CommandBuffer* commandBuffer;
    VkCommandBuffer buffer;
    DEBUG_ASSERT(vkAllocateCommandBuffers(VkGlobals::GetLogicalDevice(), &allocInfo, &buffer) == VK_SUCCESS);

    commandBuffer = &m_commandBuffers.emplace_back(buffer);
    commandBuffer->SetPool(this);

    return commandBuffer;
}

stltype::vector<CommandBuffer*> CommandPoolVulkan::CreateCommandBuffers(const CommandBufferCreateInfo& createInfo,
                                                                        const u32& count)
{
    stltype::vector<CommandBuffer*> rsltBuffers;
    rsltBuffers.reserve(count);

    u32 remainingCount = count;
    while (remainingCount > 0 && !m_freeCommandBuffers.empty())
    {
        CommandBuffer* pBuf = m_freeCommandBuffers.back();
        m_freeCommandBuffers.pop_back();
        pBuf->ResetBuffer();
        rsltBuffers.push_back(pBuf);
        remainingCount--;
    }

    if (remainingCount == 0)
        return rsltBuffers;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = createInfo.isPrimaryBuffer ? VK_COMMAND_BUFFER_LEVEL_PRIMARY : VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    allocInfo.commandBufferCount = remainingCount;

    stltype::vector<VkCommandBuffer> buffers;
    buffers.resize(remainingCount);

    DEBUG_ASSERT(vkAllocateCommandBuffers(VkGlobals::GetLogicalDevice(), &allocInfo, buffers.data()) == VK_SUCCESS);

    const size_t startIdx = m_commandBuffers.size();

    for (auto& buffer : buffers)
    {
        m_commandBuffers.emplace_back(buffer);
    }

    for (size_t i = startIdx; i < m_commandBuffers.size(); ++i)
    {
        auto* pBuffer = &m_commandBuffers[i];
        pBuffer->SetPool(this);
        rsltBuffers.emplace_back(pBuffer);
    }

    return rsltBuffers;
}

TransferCommandPoolVulkan TransferCommandPoolVulkan::Create()
{
    return {VkGlobals::GetQueueFamilyIndices().transferFamily.value()};
}
