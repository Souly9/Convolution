#pragma once
#include "Core/Global/GlobalDefines.h"
#include "BackendDefines.h"

class VertexBufferVulkan;

class RenderPassVulkan : public TrackedResource
{
public:
	static RenderPassVulkan CreateFullScreenRenderPassSimple(const RenderPassAttachment& colorAttachment);

	RenderPassVulkan() = default;
	~RenderPassVulkan();
	virtual void CleanUp() override;

	const VkRenderPass& GetRef() const;

	void SetVertexBuffer(const VertexBufferVulkan& buffer);
	void SetIndexBuffer(const IndexBufferVulkan& buffer);
	VkBuffer GetVertexBuffer() const;
	VkBuffer GetIndexBuffer() const;

private:
	VkRenderPass m_renderPass{ VK_NULL_HANDLE };
	VertexBufferVulkan m_vertexBuffer{};
	IndexBufferVulkan m_indexBuffer{};
};
