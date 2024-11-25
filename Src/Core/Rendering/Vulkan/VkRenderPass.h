#pragma once
#include "Core/Global/GlobalDefines.h"
#include "BackendDefines.h"
#include "Core/Rendering/Core/RenderPass.h"

class VertexBufferVulkan;

struct RenderPassFullScreenInfo
{
	RenderPassAttachment colorAttachment;

	RenderPassFullScreenInfo(const RenderPassAttachment& colorAttachment) : colorAttachment(colorAttachment) {}
};
struct RenderPassWithDepthFullScreenInfo : public RenderPassFullScreenInfo
{
	DepthBufferAttachmentVulkan depthAttachment;

	RenderPassWithDepthFullScreenInfo(const RenderPassAttachment& colorAttachment, const DepthBufferAttachmentVulkan& depthAttachment) : RenderPassFullScreenInfo(colorAttachment), depthAttachment(depthAttachment) {}
};

class RenderPassVulkan : public TrackedResource
{
public:
	static RenderPassVulkan CreateFullScreenRenderPassSimple(const RenderPassFullScreenInfo& colorAttachment);
	RenderPassVulkan(const RenderPassWithDepthFullScreenInfo& colorAttachment);

	RenderPassVulkan() = default;
	~RenderPassVulkan();
	virtual void CleanUp() override;

	const VkRenderPass& GetRef() const;

	void SetVertexBuffer(const VertexBufferVulkan& buffer);
	void SetIndexBuffer(const IndexBufferVulkan& buffer);

	void SetVertCountToDraw(u64 count) { m_vertCountToDraw = count; }
	u64 GetVertCount() const { return m_vertCountToDraw; }

	VkBuffer GetVertexBuffer() const;
	VkBuffer GetIndexBuffer() const;

	const stltype::vector<stltype::vector<AttachmentType>>& GetAttachmentTypes() const { return m_attachmentTypes; }
private:
	VkRenderPass m_renderPass{ VK_NULL_HANDLE };
	VertexBufferVulkan m_vertexBuffer{};
	IndexBufferVulkan m_indexBuffer{};
	stltype::vector<stltype::vector<AttachmentType>> m_attachmentTypes; // Needs to be in the right order, attachments divided by subpasses
	u64 m_vertCountToDraw{ 0 };
};
