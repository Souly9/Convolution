#include "VkBuffer.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/Utils/DeleteQueue.h"
#include "Utils/VkEnumHelpers.h"
#include "VkGlobals.h"

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
        u32 families[] = {queues.graphicsFamily.value(), queues.transferFamily.value()};
        bufferInfo.pQueueFamilyIndices = families;
    }
    m_allocatedMemory = g_pGPUMemoryManager->AllocateBuffer(info.usage, bufferInfo, m_buffer);
    m_info.size = size;
    m_info.usage = info.usage;

    // DEBUG_ASSERT(vkCreateBuffer(VK_LOGICAL_DEVICE, &bufferInfo, VulkanAllocator(), &m_buffer) == VK_SUCCESS);
    // DEBUG_ASSERT(m_buffer != VK_NULL_HANDLE);
    // VkMemoryRequirements memRequirements;
    // vkGetBufferMemoryRequirements(VK_LOGICAL_DEVICE, m_buffer, &memRequirements);

    // m_allocatedMemory = g_pGPUMemoryManager->AllocateMemory(info.size, mainBufferProperties, memRequirements);
    //
    // vkBindBufferMemory(VK_LOGICAL_DEVICE, m_buffer, m_allocatedMemory, 0);
}

void GenBufferVulkan::CleanUp()
{
    if (m_buffer == VK_NULL_HANDLE)
        return;

    auto memory = m_allocatedMemory;
    m_buffer = VK_NULL_HANDLE;

    g_pDeleteQueue->RegisterDeleteForNextFrame([memory]() mutable { g_pGPUMemoryManager->TryFreeMemory(memory); });
}

void GenBufferVulkan::FillImmediate(const void* data)
{
    CheckCopyArgs(data, UINT64_MAX, 0);
    MapAndCopyToMemory(GetMemoryHandle(), data, GetInfo().size, 0);
}

void GenBufferVulkan::FillAndTransfer(
    StagingBuffer& stgBuffer, CommandBuffer* transferBuffer, const void* data, bool freeStagingBuffer, u64 offset)
{
    CheckCopyArgs(data, UINT64_MAX, 0);
    DEBUG_ASSERT(stgBuffer.GetRef() != VK_NULL_HANDLE);

    const auto sizeToTransfer = stgBuffer.GetInfo().size;
    MapAndCopyToMemory(stgBuffer.GetMemoryHandle(), data, sizeToTransfer, offset);
    SimpleBufferCopyCmd copyCmd{stgBuffer, this};
    copyCmd.srcOffset = 0;
    copyCmd.dstOffset = offset;
    copyCmd.size = sizeToTransfer;

    if (freeStagingBuffer)
    {
        auto buffer = stgBuffer.GetRef();
        auto memory = stgBuffer.GetMemoryHandle();
        transferBuffer->AddExecutionFinishedCallback(
            [memory]() {
                g_pDeleteQueue->RegisterDeleteForNextFrame([memory]() mutable
                                                           { g_pGPUMemoryManager->TryFreeMemory(memory); });
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

void GenBufferVulkan::NamingCallBack(const stltype::string& name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
    nameInfo.objectHandle = (uint64_t)GetRef();
    nameInfo.pObjectName = name.c_str();

    vkSetDebugUtilsObjectName(VK_LOGICAL_DEVICE, &nameInfo);
}

void GenBufferVulkan::MapAndCopyToMemory(const GPUMemoryHandle& memory, const void* data, u64 size, u64 offset)
{
    const auto bufferData = g_pGPUMemoryManager->MapMemory(memory, size);
    memcpy((char*)bufferData, data, (size_t)size);
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

void StagingBuffer::CreatePersistentlyMapped(u64 size)
{
    BufferCreateInfo info{};
    info.size = size;
    info.usage = BufferUsage::Staging;
    Create(info);
    m_persistentMapping = MapMemory();
}

void StagingBuffer::CopyToMapped(const void* data, u64 size, u64 offset)
{
    DEBUG_ASSERT(m_persistentMapping != nullptr);
    memcpy((char*)m_persistentMapping + offset, data, (size_t)size);
}

void StagingBuffer::EnsureCapacity(u64 size)
{
    if (m_info.size >= size)
        return;

    if (m_persistentMapping)
    {
        UnmapMemory();
        m_persistentMapping = nullptr;
    }
    if (m_buffer != VK_NULL_HANDLE)
        CleanUp();

    CreatePersistentlyMapped(size);
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

IndirectDrawCommandBuffer::IndirectDrawCommandBuffer(u64 numOfCommands)
{
    BufferCreateInfo info{};
    info.size = sizeof(IndexedIndirectDrawCmd) * numOfCommands;
    info.usage = BufferUsage::IndirectDrawCmds;
    Create(info);
    m_indexedIndirectCmds.reserve(numOfCommands);
    // Will be reused and filled\mapped every frame so cheaper to just map forever
    m_mappedMemoryHandle = MapMemory();
}

IndirectDrawCountBuffer::IndirectDrawCountBuffer(u64 numOfCounts)
{
    BufferCreateInfo info{};
    info.size = sizeof(u32) * numOfCounts;
    info.usage = BufferUsage::IndirectDrawCount;
    Create(info);
}

void IndirectDrawCommandBuffer::AddIndexedDrawCmd(
    u32 indexCount, u32 instanceCount, u32 firstIndex, u32 vertexOffset, u32 firstInstance)
{
    if (m_indexedIndirectCmds.capacity() == m_indexedIndirectCmds.size())
    {
        // Allocate enough space for all commands from the get go please
        DEBUG_ASSERT(false);
    }
    m_indexedIndirectCmds.emplace_back(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

void IndirectDrawCommandBuffer::FillCmds()
{
    memcpy((char*)m_mappedMemoryHandle,
           (void*)m_indexedIndirectCmds.data(),
           m_indexedIndirectCmds.size() * sizeof(IndexedIndirectDrawCmd));
}

void IndirectDrawCommandBuffer::EmptyCmds()
{
    m_indexedIndirectCmds.clear();
    // memcpy((char*)m_mappedMemoryHandle, (void*)0, m_info.size);
}
