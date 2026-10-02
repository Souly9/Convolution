#pragma once
#include "MtlBackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Buffer.h"
#include "MtlGPUMemoryManager.h"


// MTL::Buffer wrapper; storage mode (Shared/Private) replaces VMA memory properties
class GenBufferMetal : public BufferBase
{
public:
    GenBufferMetal(BufferCreateInfo& info);
    virtual ~GenBufferMetal();

    void Create(BufferCreateInfo& info);

    void FillImmediate(const void* data);
    void FillImmediate(const void* data, u64 size, u64 offset);

    void FillAndTransfer(StagingBuffer& stgBuffer,
                         CommandBuffer* transferBuffer,
                         const void* data,
                         bool freeStagingBuffer = false,
                         u64 offset = 0);

    // Persistent pointer for host-visible buffers, nullptr for device-local ones
    void* GetMapped() const
    {
        return m_pMapped;
    }

    MTL::Buffer* GetRef() const
    {
        return m_buffer;
    }
    BufferCreateInfo GetInfo() const
    {
        return m_info;
    }
    BufferUsage GetUsage() const
    {
        return m_info.usage;
    }
    virtual bool IsCreated() const override
    {
        return m_buffer != nullptr;
    }

    // MTL::Buffer::gpuAddress()
    virtual u64 GetDeviceAddress() const override;

protected:
    GenBufferMetal()
    {
    }

    BufferCreateInfo m_info{};
    MTL::Buffer* m_buffer{nullptr};
    void* m_pMapped{nullptr}; // TODO(metal): contents() for shared storage
};

class VertexBufferMetal : public GenBufferMetal
{
public:
    VertexBufferMetal(u64 size, bool hostVisible = false);
    VertexBufferMetal()
    {
    }
};

class IndexBufferMetal : public GenBufferMetal
{
public:
    IndexBufferMetal(u64 size, bool hostVisible = false);
    IndexBufferMetal()
    {
    }
};

class UniformBufferMetal : public GenBufferMetal
{
public:
    UniformBufferMetal(u64 size);
    UniformBufferMetal()
    {
    }
};

class StagingBufferMetal : public GenBufferMetal
{
public:
    StagingBufferMetal()
    {
    }
    StagingBufferMetal(u64 size);

    void CopyToMapped(const void* data, u64 size, u64 offset = 0);

    void EnsureCapacity(u64 size);
};

class StorageBufferMetal : public GenBufferMetal
{
public:
    StorageBufferMetal(u64 size, bool isDevice = false);
    StorageBufferMetal()
    {
    }
};

// Metal has no DrawIndexedIndirectCount; GPU-driven draws need an ICB or per-draw indirect calls
class IndirectDrawCommandBufferMetal : public GenBufferMetal
{
public:
    explicit IndirectDrawCommandBufferMetal(u64 numOfCommands);
    IndirectDrawCommandBufferMetal()
    {
    }

    void Init(u64 numOfCommands);
    void AddIndexedDrawCmd(u32 indexCount, u32 instanceCount, u32 firstIndex, u32 vertexOffset, u32 firstInstance);

    void FillCmds();

    void EmptyCmds();

    u32 GetDrawCmdNum() const
    {
        return m_indexedIndirectCmds.size();
    }

protected:
    stltype::vector<IndexedIndirectDrawCmd> m_indexedIndirectCmds;
};

class IndirectDrawCountBuffer : public GenBufferMetal
{
public:
    IndirectDrawCountBuffer(u64 numOfCounts);
    IndirectDrawCountBuffer()
    {
    }

    void Init(u64 numOfCounts);
};
