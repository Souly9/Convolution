#pragma once
#include "Core/Global/GlobalDefines.h"
#include "BackendDefines.h"
#include "Core/Rendering/Core/CommandBuffer.h"

class CBufferVulkan : public CBuffer
{
public:
	CBufferVulkan() = default;
	CBufferVulkan(VkCommandBuffer commandBuffer) : m_commandBuffer(commandBuffer) {}
	~CBufferVulkan();

	void Bake(); 

	const VkCommandBuffer& GetRef() const { return m_commandBuffer; }
	VkCommandBuffer& GetRef() { return m_commandBuffer; }
	void SetPool(CommandPoolVulkan* pool) { m_pool = pool; }

	void BeginBuffer();
	void BeginBufferForSingleSubmit();
	void BeginRPass(GenericDrawCmd& cmd);
	void EndRPass();
	void EndBuffer();

	void ResetBuffer();
	void ReturnToPool();

	void Destroy(VkCommandPool pool);

protected:
	CommandPoolVulkan* m_pool;
	VkCommandBuffer  m_commandBuffer;
};