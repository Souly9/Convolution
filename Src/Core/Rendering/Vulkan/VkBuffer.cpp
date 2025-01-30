#include "VkGlobals.h"
#include "Utils/VkEnumHelpers.h"
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
    DEBUG_ASSERT(m_buffer != VK_NULL_HANDLE);

    m_info.size = size;
    m_info.usage = info.usage;

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(VK_LOGICAL_DEVICE, m_buffer, &memRequirements);
    VkMemoryPropertyFlags mainBufferProperties = Conv2MemFlags(info.usage);

    m_allocatedMemory = g_pGPUMemoryManager->AllocateMemory(info.size, mainBufferProperties, memRequirements);
    
    vkBindBufferMemory(VK_LOGICAL_DEVICE, m_buffer, m_allocatedMemory, 0);
}

void GenBufferVulkan::CleanUp()
{
    auto bufferHandle = m_buffer;
    auto memory = m_allocatedMemory;

    g_pDeleteQueue->RegisterDeleteForNextFrame([bufferHandle, memory]() mutable
        {
            vkDestroyBuffer(VK_LOGICAL_DEVICE, bufferHandle, VulkanAllocator());
            g_pGPUMemoryManager->TryFreeMemory(memory);
        });
}

void GenBufferVulkan::FillImmediate(const void* data)
{
    CheckCopyArgs(data, UINT64_MAX, 0);
    MapAndCopyToMemory(GetMemoryHandle(), data, GetInfo().size, 0);
}

void GenBufferVulkan::FillAndTransfer(StagingBuffer& stgBuffer, CommandBuffer* transferBuffer, const void* data, bool freeStagingBuffer, u64 offset)
{
    CheckCopyArgs(data, UINT64_MAX, 0);
    DEBUG_ASSERT(stgBuffer.GetRef() != VK_NULL_HANDLE);

    MapAndCopyToMemory(stgBuffer.GetMemoryHandle(), data, stgBuffer.GetInfo().size, offset);
    SimpleBufferCopyCmd copyCmd{&stgBuffer, this};
	copyCmd.srcOffset = 0;
	copyCmd.dstOffset = offset;
	copyCmd.size = m_info.size;

    if (freeStagingBuffer)
    {
        auto buffer = stgBuffer.GetRef();
        auto memory = stgBuffer.GetMemoryHandle();
        transferBuffer->AddExecutionFinishedCallback([buffer, memory]()
            {
                vkDestroyBuffer(VK_LOGICAL_DEVICE, buffer, VulkanAllocator());
                g_pGPUMemoryManager->TryFreeMemory(memory);
            });

        // Guarantee it won't get freed until we hit the callback
        stgBuffer.Grab();
    }

    transferBuffer->RecordCommand(copyCmd);
}

GPUMappedMemoryHandle GenBufferVulkan::MapMemory()
{
    return g_pGPUMemoryManager->MapMemory(m_allocatedMemory, m_info.size);
}

void GenBufferVulkan::UnmapMemory()
{
    g_pGPUMemoryManager->UnmapMemory(m_allocatedMemory);
}

void GenBufferVulkan::MapAndCopyToMemory(const GPUMemoryHandle& memory, const void* data, u64 size, u64 offset)
{
    const auto bufferData = g_pGPUMemoryManager->MapMemory(memory, m_info.size);
    memcpy((char*)bufferData, data, (size_t)m_info.size);
    g_pGPUMemoryManager->UnmapMemory(memory);
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

IndexBufferVulkan::IndexBufferVulkan(u64 size)
{
    BufferCreateInfo info{};
    info.size = size;
    info.usage = BufferUsage::Index;
    Create(info);
}

UniformBuffer::UniformBuffer(u64 size)
{
    BufferCreateInfo info{};
    info.size = size;
    info.usage = BufferUsage::Uniform;
    Create(info);
}

StorageBuffer::StorageBuffer(u64 size, bool isDevice)
{
    BufferCreateInfo info{};
    info.size = size;
    info.usage = isDevice ? BufferUsage::SSBODevice : BufferUsage::SSBOHost;
    Create(info);
}
