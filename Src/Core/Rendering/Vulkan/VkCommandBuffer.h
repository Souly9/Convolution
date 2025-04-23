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
	void BeginRPass(BeginRPassCmd& cmd);
	void BeginRPassGeneric(const DrawCmdDummy& cmd);

	void EndRPass();
	void EndBuffer();

	void ResetBuffer();

	void Destroy();

protected:
	CommandPoolVulkan* m_pool{ nullptr };
	VkCommandBuffer  m_commandBuffer{VK_NULL_HANDLE};
};