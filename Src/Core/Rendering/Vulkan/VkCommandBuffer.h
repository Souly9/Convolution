#pragma once
#include "Core/Global/GlobalDefines.h"
#include "BackendDefines.h"
#include "Core/Rendering/Core/CommandBuffer.h"

class CBufferVulkan : public CBuffer
{
public:
	CBufferVulkan() = default;
	CBufferVulkan(VkCommandBuffer commandBuffer) : m_commandBuffer(commandBuffer) {}

	void Bake(); 

	const VkCommandBuffer& GetRef() const { return m_commandBuffer; }
	VkCommandBuffer& GetRef() { return m_commandBuffer; }

	void BeginBuffer();
	void BeginRPass(GenericDrawCmd& cmd);
	void EndRPass();
	void EndBuffer();

	void ResetBuffer();

	void Destroy(VkCommandPool pool);

protected:

	VkCommandBuffer  m_commandBuffer;
};