#include "VkGlobals.h"
#include "VkEnumHelpers.h"
#include "VkBuffer.h"

GenBufferVulkan::GenBufferVulkan(BufferCreateInfo& info)
{
    Create(info);
}

GenBufferVulkan::~GenBufferVulkan()
{
    TRACKED_DESC_IMPL
}

void GenBufferVulkan::Create(BufferCreateInfo& info)
{
    const auto size = info.size;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = Conv(info.usage);
    bufferInfo.sharingMode = info.isExclusive ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT;
    if (info.isExclusive)
    {
        bufferInfo.queueFamilyIndexCount = info.isExclusive ? 0 : 2;
        const auto& queues = VkGlobals::GetQueueFamilyIndices();
        u32 families[] = { queues.graphicsFamily.value(), queues.transferFamily.value() };
        bufferInfo.pQueueFamilyIndices = families;
    }

    DEBUG_ASSERT(vkCreateBuffer(VK_LOGICAL_DEVICE, &bufferInfo, VulkanAllocator(), &m_buffer) == VK_SUCCESS);

    m_info.size = size;

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(VK_LOGICAL_DEVICE, m_buffer, &memRequirements);
    VkMemoryPropertyFlags mainBufferProperties = Conv2MemFlags(info.usage);

    m_allocatedMemory = g_pGPUMemoryManager->AllocateMemory(info.size, mainBufferProperties, memRequirements);
    
    vkBindBufferMemory(VK_LOGICAL_DEVICE, m_buffer, m_allocatedMemory, 0);
}

void GenBufferVulkan::CleanUp()
{
    vkDestroyBuffer(VK_LOGICAL_DEVICE, m_buffer, VulkanAllocator());
    m_buffer = VK_NULL_HANDLE;
}

void GenBufferVulkan::FillImmediate(const void* data)
{
    CheckCopyArgs(data, UINT64_MAX, 0);
    MapAndCopyToMemory(m_allocatedMemory, data, m_info.size, 0);
}

void GenBufferVulkan::FillAndTransfer(StagingBuffer& stgBuffer, CommandBuffer* transferBuffer, const void* data, bool freeStagingBuffer )
{
    CheckCopyArgs(data, UINT64_MAX, 0);
    DEBUG_ASSERT(stgBuffer.GetRef() != VK_NULL_HANDLE);

    MapAndCopyToMemory(stgBuffer.GetMemoryHandle(), data, stgBuffer.GetInfo().size, 0);
    SimpleBufferCopyCmd copyCmd{&stgBuffer, this};
	copyCmd.srcOffset = 0;
	copyCmd.dstOffset = 0;
	copyCmd.size = m_info.size;

    if (freeStagingBuffer)
    {
        auto callback = [&stgBuffer]() { 
            stgBuffer.CleanUp(); 
            };
        copyCmd.optionalCallback = std::bind(callback);

        // Guarantee it won't get freed until we hit the callback
        stgBuffer.Grab();
    }

    transferBuffer->RecordCommand(copyCmd);
}

void GenBufferVulkan::MapAndCopyToMemory(const GPUMemoryHandle& memory, const void* data, u64 size, u64 offset)
{
    const auto& device = VK_LOGICAL_DEVICE;
    void* bufferData;
    vkMapMemory(device, memory, 0, m_info.size, 0, &bufferData);
    memcpy(bufferData, data, (size_t)m_info.size);
    vkUnmapMemory(device, memory);
}

void GenBufferVulkan::CheckCopyArgs(const void* data, u64 size, u64 offset)
{
    DEBUG_ASSERT(m_allocatedMemory != VK_NULL_HANDLE);
    DEBUG_ASSERT(data != nullptr);
    DEBUG_ASSERT(m_info.size <= size);
}

VertexBufferVulkan::VertexBufferVulkan(u64 size)
{
    BufferCreateInfo info{};
	info.size = size;
    info.usage = BufferUsage::Vertex;
    Create(info);
}

StagingBuffer::StagingBuffer(u64 size)
{
    BufferCreateInfo info{};
    info.size = size;
    info.usage = BufferUsage::Staging;
    Create(info);
}
