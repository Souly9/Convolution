#pragma once
#include "BackendDefines.h"
#include "ConvAllocatorSimple.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/Buffer.h"

class GenBufferVulkan : public BufferBase
{
public:
    GenBufferVulkan(BufferCreateInfo& info);
    virtual ~GenBufferVulkan();

    void Create(BufferCreateInfo& info);

    virtual void CleanUp() override;

    // Fill buffer without trying to copy memory into device memory using staging buffer
    void FillImmediate(const void* data);
    void FillImmediate(const void* data, u64 size, u64 offset);

    // Fill buffer using staging buffer
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

    VkBuffer GetRef() const
    {
        return m_buffer;
    }
    GPUMemoryHandle GetMemoryHandle() const
    {
        return m_allocatedMemory;
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
        return m_buffer != VK_NULL_HANDLE;
    }

    virtual u64 GetDeviceAddress() const override;

    virtual void NamingCallBack(const stltype::string& name) override;

protected:
    GenBufferVulkan()
    {
    }

    void CheckCopyArgs(const void* data, u64 size, u64 offset);
    BufferCreateInfo m_info{};
    VkBuffer m_buffer{VK_NULL_HANDLE};
    GPUMemoryHandle m_allocatedMemory{VK_NULL_HANDLE};
    void* m_pMapped{nullptr};
};

class VertexBufferVulkan : public GenBufferVulkan
{
public:
    // hostVisible for small buffers filled with FillImmediate; scene geometry goes through the upload queue
    VertexBufferVulkan(u64 size, bool hostVisible = false);
    VertexBufferVulkan()
    {
    }
};

class IndexBufferVulkan : public GenBufferVulkan
{
public:
    IndexBufferVulkan(u64 size, bool hostVisible = false);
    IndexBufferVulkan()
    {
    }
};

class UniformBufferVulkan : public GenBufferVulkan
{
public:
    UniformBufferVulkan(u64 size);
    UniformBufferVulkan()
    {
    }
};

class StagingBufferVulkan : public GenBufferVulkan
{
public:
    StagingBufferVulkan() {}
    StagingBufferVulkan(u64 size);

    void CopyToMapped(const void* data, u64 size, u64 offset = 0);

    // Recreate if current capacity is too small, reuse otherwise
    void EnsureCapacity(u64 size);
};

class StorageBufferVulkan : public GenBufferVulkan
{
public:
    StorageBufferVulkan(u64 size, bool isDevice = false);
    StorageBufferVulkan()
    {
    }
};

// Also has some utility data and functions to unify the management of command array and buffer
class IndirectDrawCommandBufferVulkan : public GenBufferVulkan
{
public:
    explicit IndirectDrawCommandBufferVulkan(u64 numOfCommands);
    IndirectDrawCommandBufferVulkan()
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

class IndirectDrawCountBuffer : public GenBufferVulkan
{
public:
    IndirectDrawCountBuffer(u64 numOfCounts);
    IndirectDrawCountBuffer()
    {
    }

    void Init(u64 numOfCounts);
};
