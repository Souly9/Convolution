#include "VkCommandPool.h"
#include "Core/Global/GlobalDefines.h"
#include "VkGlobals.h"

#define MAX_REASONABLE_COMMAND_BUFFERS 2048

void CommandPoolVulkan::ReturnCommandBuffer(const CommandBuffer* pBuffer)
{
    if (pBuffer == nullptr)
        return;
    m_commandBuffers.erase(stltype::remove_if(m_commandBuffers.begin(),
                                              m_commandBuffers.end(),
                                              [pBuffer](const CommandBuffer& buffer)
                                              { return buffer.GetRef() == pBuffer->GetRef(); }),
                           m_commandBuffers.end());
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

    // Store the starting index before adding buffers
    const size_t startIdx = m_commandBuffers.size();

    // First, add all buffers to the vector
    for (auto& buffer : buffers)
    {
        m_commandBuffers.emplace_back(buffer);
    }

    // Now collect pointers - the vector is stable at this point
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
