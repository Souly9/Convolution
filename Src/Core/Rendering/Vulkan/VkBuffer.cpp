#include "VkBuffer.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/Utils/DeleteQueue.h"
#include "Utils/VkEnumHelpers.h"
#include "VkBackendAccess.h"

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
        const auto& queues = VkBackend::QueueFamilies();
        u32 families[] = {queues.graphicsFamily.value(), queues.transferFamily.value()};
        bufferInfo.pQueueFamilyIndices = families;
    }
    m_allocatedMemory = g_renderer.GetGPUMemoryManager().AllocateBuffer(info.usage, bufferInfo, m_buffer);
    m_info.size = size;
    m_info.usage = info.usage;

    // DEBUG_ASSERT(vkCreateBuffer(VkBackend::Device(), &bufferInfo, VulkanAllocator(), &m_buffer) == VK_SUCCESS);
    // DEBUG_ASSERT(m_buffer != VK_NULL_HANDLE);
    // VkMemoryRequirements memRequirements;
    // vkGetBufferMemoryRequirements(VkBackend::Device(), m_buffer, &memRequirements);

    // m_allocatedMemory = g_renderer.GetGPUMemoryManager().AllocateMemory(info.size, mainBufferProperties, memRequirements);
    //
    // vkBindBufferMemory(VkBackend::Device(), m_buffer, m_allocatedMemory, 0);
}

void GenBufferVulkan::CleanUp()
{
    if (m_buffer == VK_NULL_HANDLE)
        return;

    auto memory = m_allocatedMemory;
    m_buffer = VK_NULL_HANDLE;

    g_renderer.GetDeleteQueue().RegisterDeleteForNextFrame([memory]() mutable { g_renderer.GetGPUMemoryManager().TryFreeMemory(memory); });
}

void GenBufferVulkan::FillImmediate(const void* data)
{
    CheckCopyArgs(data, UINT64_MAX, 0);
    MapAndCopyToMemory(GetMemoryHandle(), data, GetInfo().size, 0);
}

void GenBufferVulkan::FillImmediate(const void* data, u64 size, u64 offset)
{
    CheckCopyArgs(data, UINT64_MAX, 0);
    MapAndCopyToMemory(GetMemoryHandle(), data, size, offset);
}

void GenBufferVulkan::FillAndTransfer(
    StagingBuffer& stgBuffer, CommandBuffer* transferBuffer, const void* data, bool freeStagingBuffer, u64 offset)
{
    CheckCopyArgs(data, UINT64_MAX, 0);
    DEBUG_ASSERT(stgBuffer.GetRef() != VK_NULL_HANDLE);

    const auto sizeToTransfer = stgBuffer.GetInfo().size;
    MapAndCopyToMemory(stgBuffer.GetMemoryHandle(), data, sizeToTransfer, offset);
    SimpleBufferCopyCmd copyCmd{&stgBuffer, this};
    copyCmd.srcOffset = 0;
    copyCmd.dstOffset = offset;
    copyCmd.size = sizeToTransfer;

    if (freeStagingBuffer)
    {
        auto buffer = stgBuffer.GetRef();
        auto memory = stgBuffer.GetMemoryHandle();
        transferBuffer->AddExecutionFinishedCallback(
            [memory]() {
                g_renderer.GetDeleteQueue().RegisterDeleteForNextFrame([memory]() mutable
                                                           { g_renderer.GetGPUMemoryManager().TryFreeMemory(memory); });
            });

        // Guarantee it won't get freed until we hit the callback
        stgBuffer.Grab();
    }

    transferBuffer->RecordCommand(copyCmd);
}

GPUMappedMemoryHandle GenBufferVulkan::MapMemory()
{
    return g_renderer.GetGPUMemoryManager().MapMemory(m_allocatedMemory, m_info.size);
}

u64 GenBufferVulkan::GetDeviceAddress() const
{
    if (m_buffer == VK_NULL_HANDLE)
        return 0;

    VkBufferDeviceAddressInfo addressInfo{};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = m_buffer;
    return vkGetBufferDeviceAddress(VkBackend::Device(), &addressInfo);
}

void GenBufferVulkan::UnmapMemory()
{
    g_renderer.GetGPUMemoryManager().UnmapMemory(m_allocatedMemory);
}

void GenBufferVulkan::NamingCallBack(const stltype::string& name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
    nameInfo.objectHandle = (uint64_t)GetRef();
    nameInfo.pObjectName = name.c_str();

    vkSetDebugUtilsObjectName(VkBackend::Device(), &nameInfo);
}

void GenBufferVulkan::MapAndCopyToMemory(const GPUMemoryHandle& memory, const void* data, u64 size, u64 offset)
{
    const auto bufferData = g_renderer.GetGPUMemoryManager().MapMemory(memory, size);
    memcpy((char*)bufferData + offset, data, (size_t)size);
    g_renderer.GetGPUMemoryManager().UnmapMemory(memory);
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

StagingBufferVulkan::StagingBufferVulkan(u64 size)
{
    BufferCreateInfo info{};
    info.size = size;
    info.usage = BufferUsage::Staging;
    Create(info);
}

void StagingBufferVulkan::CreatePersistentlyMapped(u64 size)
{
    BufferCreateInfo info{};
    info.size = size;
    info.usage = BufferUsage::Staging;
    Create(info);
    m_persistentMapping = MapMemory();
}

void StagingBufferVulkan::CopyToMapped(const void* data, u64 size, u64 offset)
{
    DEBUG_ASSERT(m_persistentMapping != nullptr);
    memcpy((char*)m_persistentMapping + offset, data, (size_t)size);
}

void StagingBufferVulkan::EnsureCapacity(u64 size)
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

UniformBufferVulkan::UniformBufferVulkan(u64 size)
{
    BufferCreateInfo info{};
    info.size = size;
    info.usage = BufferUsage::Uniform;
    Create(info);
}

StorageBufferVulkan::StorageBufferVulkan(u64 size, bool isDevice)
{
    BufferCreateInfo info{};
    info.size = size;
    info.usage = isDevice ? BufferUsage::SSBODevice : BufferUsage::SSBOHost;
    Create(info);
}

IndirectDrawCommandBufferVulkan::IndirectDrawCommandBufferVulkan(u64 numOfCommands)
{
    BufferCreateInfo info{};
    info.size = sizeof(IndexedIndirectDrawCmd) * numOfCommands;
    info.usage = BufferUsage::IndirectDrawCmds;
    Create(info);
    m_indexedIndirectCmds.reserve(numOfCommands);
    // Will be reused and filled\mapped every frame so cheaper to just map forever
    m_mappedMemoryHandle = MapMemory();
}

void IndirectDrawCommandBufferVulkan::Init(u64 numOfCommands)
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
    Init(numOfCounts);
}

void IndirectDrawCountBuffer::Init(u64 numOfCounts)
{
    BufferCreateInfo info{};
    info.size = sizeof(u32) * numOfCounts;
    info.usage = BufferUsage::IndirectDrawCount;
    Create(info);
}

void IndirectDrawCommandBufferVulkan::AddIndexedDrawCmd(
    u32 indexCount, u32 instanceCount, u32 firstIndex, u32 vertexOffset, u32 firstInstance)
{
    if (m_indexedIndirectCmds.capacity() == m_indexedIndirectCmds.size())
    {
        // Allocate enough space for all commands from the get go please
        DEBUG_ASSERT(false);
    }
    m_indexedIndirectCmds.push_back({indexCount, instanceCount, firstIndex, (s32)vertexOffset, firstInstance});
}

void IndirectDrawCommandBufferVulkan::FillCmds()
{
    memcpy((char*)m_mappedMemoryHandle,
           (void*)m_indexedIndirectCmds.data(),
           m_indexedIndirectCmds.size() * sizeof(IndexedIndirectDrawCmd));
}

void IndirectDrawCommandBufferVulkan::EmptyCmds()
{
    m_indexedIndirectCmds.clear();
    // memcpy((char*)m_mappedMemoryHandle, (void*)0, m_info.size);
}
