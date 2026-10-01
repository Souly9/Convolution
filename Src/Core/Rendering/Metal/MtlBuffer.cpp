#include "MtlBuffer.h"

// TODO(Metal): implement with MTL::Device::newBuffer and storage modes

GenBufferMetal::GenBufferMetal(BufferCreateInfo& info)
{
    Create(info);
}

GenBufferMetal::~GenBufferMetal()
{
    TRACKED_DESC_IMPL
}

void GenBufferMetal::Create(BufferCreateInfo& info)
{
    m_info.size = info.size;
    m_info.usage = info.usage;
}

void GenBufferMetal::CleanUp()
{
}

void GenBufferMetal::FillImmediate(const void* data)
{
}

void GenBufferMetal::FillImmediate(const void* data, u64 size, u64 offset)
{
}

void GenBufferMetal::FillAndTransfer(
    StagingBuffer& stgBuffer, CommandBuffer* transferBuffer, const void* data, bool freeStagingBuffer, u64 offset)
{
}

GPUMappedMemoryHandle GenBufferMetal::MapMemory()
{
    return nullptr;
}

void GenBufferMetal::UnmapMemory()
{
}

u64 GenBufferMetal::GetDeviceAddress() const
{
    return 0;
}

void GenBufferMetal::NamingCallBack(const stltype::string& name)
{
}

VertexBufferMetal::VertexBufferMetal(u64 size, bool hostVisible)
{
}

IndexBufferMetal::IndexBufferMetal(u64 size, bool hostVisible)
{
}

UniformBufferMetal::UniformBufferMetal(u64 size)
{
}

StagingBufferMetal::StagingBufferMetal(u64 size)
{
}

void StagingBufferMetal::CreatePersistentlyMapped(u64 size)
{
}

void StagingBufferMetal::CopyToMapped(const void* data, u64 size, u64 offset)
{
}

void StagingBufferMetal::EnsureCapacity(u64 size)
{
}

StorageBufferMetal::StorageBufferMetal(u64 size, bool isDevice)
{
}

IndirectDrawCommandBufferMetal::IndirectDrawCommandBufferMetal(u64 numOfCommands)
{
    Init(numOfCommands);
}

void IndirectDrawCommandBufferMetal::Init(u64 numOfCommands)
{
}

void IndirectDrawCommandBufferMetal::AddIndexedDrawCmd(
    u32 indexCount, u32 instanceCount, u32 firstIndex, u32 vertexOffset, u32 firstInstance)
{
    m_indexedIndirectCmds.push_back(
        {indexCount, instanceCount, firstIndex, static_cast<s32>(vertexOffset), firstInstance});
}

void IndirectDrawCommandBufferMetal::FillCmds()
{
}

void IndirectDrawCommandBufferMetal::EmptyCmds()
{
    m_indexedIndirectCmds.clear();
}

IndirectDrawCountBuffer::IndirectDrawCountBuffer(u64 numOfCounts)
{
    Init(numOfCounts);
}

void IndirectDrawCountBuffer::Init(u64 numOfCounts)
{
}
