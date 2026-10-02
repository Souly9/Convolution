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
    // The build-input bit needs VK_KHR_acceleration_structure, which devices without ray tracing (MoltenVK) lack
    if (!g_renderer.SupportsRayTracing())
        bufferInfo.usage &= ~VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
    VkBackend::SetSharedQueueFamilies(bufferInfo);
    m_allocatedMemory = g_renderer.GetGPUMemoryManager().AllocateBuffer(info.usage, bufferInfo, m_buffer, m_pMapped);
    m_info.size = size;
    m_info.usage = info.usage;
}

void GenBufferVulkan::CleanUp()
{
    if (m_buffer == VK_NULL_HANDLE)
        return;

    auto memory = m_allocatedMemory;
    m_buffer = VK_NULL_HANDLE;
    m_pMapped = nullptr;

    g_renderer.GetDeleteQueue().RegisterDeleteForNextFrame([memory]()
                                                           { g_renderer.GetGPUMemoryManager().Free(memory); });
}

void GenBufferVulkan::FillImmediate(const void* data)
{
    CheckCopyArgs(data, UINT64_MAX, 0);
    memcpy(m_pMapped, data, m_info.size);
}

void GenBufferVulkan::FillImmediate(const void* data, u64 size, u64 offset)
{
    CheckCopyArgs(data, UINT64_MAX, 0);
    memcpy(static_cast<u8*>(m_pMapped) + offset, data, size);
}

void GenBufferVulkan::FillAndTransfer(
    StagingBuffer& stgBuffer, CommandBuffer* transferBuffer, const void* data, bool freeStagingBuffer, u64 offset)
{
    CheckCopyArgs(data, UINT64_MAX, 0);
    DEBUG_ASSERT(stgBuffer.GetRef() != VK_NULL_HANDLE);

    const auto sizeToTransfer = stgBuffer.GetInfo().size;
    memcpy(static_cast<u8*>(stgBuffer.GetMapped()) + offset, data, sizeToTransfer);
    SimpleBufferCopyCmd copyCmd{&stgBuffer, this};
    copyCmd.srcOffset = 0;
    copyCmd.dstOffset = offset;
    copyCmd.size = sizeToTransfer;

    if (freeStagingBuffer)
    {
        auto buffer = stgBuffer.GetRef();
        auto memory = stgBuffer.GetMemoryHandle();
        transferBuffer->AddExecutionFinishedCallback(
            [memory]()
            {
                g_renderer.GetDeleteQueue().RegisterDeleteForNextFrame(
                    [memory]() { g_renderer.GetGPUMemoryManager().Free(memory); });
            });

        // Guarantee it won't get freed until we hit the callback
        stgBuffer.Grab();
    }

    transferBuffer->RecordCommand(copyCmd);
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

void GenBufferVulkan::NamingCallBack(const stltype::string& name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
    nameInfo.objectHandle = (uint64_t)GetRef();
    nameInfo.pObjectName = name.c_str();

    vkSetDebugUtilsObjectName(VkBackend::Device(), &nameInfo);
    g_renderer.GetGPUMemoryManager().SetName(m_allocatedMemory, name);
}

void GenBufferVulkan::CheckCopyArgs(const void* data, u64 size, u64 offset)
{
    DEBUG_ASSERT(m_allocatedMemory != VK_NULL_HANDLE);
    DEBUG_ASSERT(data != nullptr);
    DEBUG_ASSERT(m_info.size <= size);
}

VertexBufferVulkan::VertexBufferVulkan(u64 size, bool hostVisible)
{
    BufferCreateInfo info{};
    info.size = size;
    info.usage = hostVisible ? BufferUsage::VertexHost : BufferUsage::Vertex;
    Create(info);
}

StagingBufferVulkan::StagingBufferVulkan(u64 size)
{
    BufferCreateInfo info{};
    info.size = size;
    info.usage = BufferUsage::Staging;
    Create(info);
}

void StagingBufferVulkan::CopyToMapped(const void* data, u64 size, u64 offset)
{
    memcpy(static_cast<u8*>(m_pMapped) + offset, data, size);
}

void StagingBufferVulkan::EnsureCapacity(u64 size)
{
    if (m_info.size >= size)
        return;

    CleanUp();
    BufferCreateInfo info{};
    info.size = size;
    info.usage = BufferUsage::Staging;
    Create(info);
}

IndexBufferVulkan::IndexBufferVulkan(u64 size, bool hostVisible)
{
    BufferCreateInfo info{};
    info.size = size;
    info.usage = hostVisible ? BufferUsage::IndexHost : BufferUsage::Index;
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
}

void IndirectDrawCommandBufferVulkan::Init(u64 numOfCommands)
{
    BufferCreateInfo info{};
    info.size = sizeof(IndexedIndirectDrawCmd) * numOfCommands;
    info.usage = BufferUsage::IndirectDrawCmds;
    Create(info);
    m_indexedIndirectCmds.reserve(numOfCommands);
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
    memcpy(
        m_pMapped, (void*)m_indexedIndirectCmds.data(), m_indexedIndirectCmds.size() * sizeof(IndexedIndirectDrawCmd));
}

void IndirectDrawCommandBufferVulkan::EmptyCmds()
{
    m_indexedIndirectCmds.clear();
}
