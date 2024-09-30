#pragma once
#include "Core/Global/GlobalDefines.h"
#include "BackendDefines.h"
#include "VkGPUMemoryManager.h"
#include "Core/Rendering/Core/Buffer.h"

struct BufferInfo
{
	u64 size;

};
class VertexBufferVulkan;
class StagingBuffer;

class GenBufferVulkan : public GenBuffer
{
public:
	GenBufferVulkan(BufferCreateInfo& info);
	~GenBufferVulkan();

	void Create(BufferCreateInfo& info);

	virtual void CleanUp() override;

	// Fill buffer without trying to copy memory into device memory using staging buffer
	void FillImmediate(const void* data);

	// Fill buffer using staging buffer
	void FillAndTransfer(StagingBuffer& stgBuffer, CommandBuffer* transferBuffer, const void* data, bool freeStagingBuffer = false);

	VkBuffer GetRef() const { return m_buffer; }
	GPUMemoryHandle GetMemoryHandle() const { return m_allocatedMemory; }
	BufferInfo GetInfo() const { return m_info; }

protected:
	GenBufferVulkan() {}

	void MapAndCopyToMemory(const GPUMemoryHandle& memory, const void* data, u64 size, u64 offset);
	void CheckCopyArgs(const void* data, u64 size, u64 offset);

	BufferInfo m_info{};
	VkBuffer m_buffer{ VK_NULL_HANDLE };
	GPUMemoryHandle m_allocatedMemory { VK_NULL_HANDLE };
};

class VertexBufferVulkan : public GenBufferVulkan
{
public:
	VertexBufferVulkan(u64 size);
	VertexBufferVulkan() {}

};

class StagingBuffer : public GenBufferVulkan
{
public:
	StagingBuffer(u64 size);

private:
};