#pragma once
#include "BackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/Buffer.h"
#include "VkGPUMemoryManager.h"

struct BufferInfo
{
    u64 size{0};
    BufferUsage usage;
};
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

    GPUMappedMemoryHandle MapMemory();
    void UnmapMemory();

    VkBuffer GetRef() const
    {
        return m_buffer;
    }
    GPUMemoryHandle GetMemoryHandle() const
    {
        return m_allocatedMemory;
    }
    BufferInfo GetInfo() const
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

    void MapAndCopyToMemory(const GPUMemoryHandle& memory, const void* data, u64 size, u64 offset);
    void CheckCopyArgs(const void* data, u64 size, u64 offset);
    BufferInfo m_info{};
    VkBuffer m_buffer{VK_NULL_HANDLE};
    GPUMemoryHandle m_allocatedMemory{VK_NULL_HANDLE};
};

class VertexBufferVulkan : public GenBufferVulkan
{
public:
    VertexBufferVulkan(u64 size);
    VertexBufferVulkan()
    {
    }
};

class IndexBufferVulkan : public GenBufferVulkan
{
public:
    IndexBufferVulkan(u64 size);
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

    void CreatePersistentlyMapped(u64 size);
    void CopyToMapped(const void* data, u64 size, u64 offset = 0);
    GPUMappedMemoryHandle GetPersistentMapping() const { return m_persistentMapping; }

    // Recreate if current capacity is too small, reuse otherwise
    void EnsureCapacity(u64 size);

private:
    GPUMappedMemoryHandle m_persistentMapping{nullptr};
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
    GPUMappedMemoryHandle m_mappedMemoryHandle;
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
