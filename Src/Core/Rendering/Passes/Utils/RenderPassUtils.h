#pragma once
#include "Core/Rendering/Core/Defines/VertexDefines.h"
#include "../PassManager.h"

struct WriteableRTAttachments
{
	ColorAttachmentVulkan color;
	DepthBufferAttachmentVulkan depth;
};
static inline ColorAttachment CreateDefaultColorAttachment(TexFormat format, LoadOp loadOp, Texture* pTex)
{
	ColorAttachmentInfo colorAttachmentInfo{};
	colorAttachmentInfo.format = format;
	colorAttachmentInfo.loadOp = loadOp;
	colorAttachmentInfo.storeOp = StoreOp::STORE;
	colorAttachmentInfo.initialLayout = ImageLayout::UNDEFINED;
	colorAttachmentInfo.finalLayout = ImageLayout::COLOR_ATTACHMENT;

	return ColorAttachment::Create(colorAttachmentInfo, pTex);
}
static inline DepthAttachment CreateDefaultDepthAttachment(TexFormat format, LoadOp loadOp, Texture* pTex)
{
	DepthBufferAttachmentInfo depthAttachmentInfo{};
	depthAttachmentInfo.format = format;
	depthAttachmentInfo.loadOp = loadOp;
	depthAttachmentInfo.stencilLoadOp = loadOp;
	depthAttachmentInfo.initialLayout = ImageLayout::UNDEFINED;
	depthAttachmentInfo.finalLayout = ImageLayout::DEPTH_STENCIL;

	return DepthAttachment::Create(depthAttachmentInfo, pTex);
}
static inline DepthAttachment CreateDefaultDepthAttachment(LoadOp loadOp, Texture* pTex)
{
	return CreateDefaultDepthAttachment(DEPTH_BUFFER_FORMAT, loadOp, pTex);
}

static inline bool NeedToRender(const IndirectDrawCommandBuffer& buffer)
{
	if (buffer.GetDrawCmdNum() == 0)
		return false;
	return true;
}