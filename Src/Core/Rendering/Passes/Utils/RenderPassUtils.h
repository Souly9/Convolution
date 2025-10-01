#pragma once
#include "Core/Rendering/Core/Defines/VertexDefines.h"
#include "../PassManager.h"

static inline MinVertex ConvertVertexFormat(const CompleteVertex& completeVert)
{
	return { completeVert.position };
}

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
static inline DepthAttachment CreateDefaultDepthAttachment(LoadOp loadOp, Texture* pTex)
{
	DepthBufferAttachmentInfo depthAttachmentInfo{};
	depthAttachmentInfo.format = DEPTH_BUFFER_FORMAT;
	depthAttachmentInfo.loadOp = loadOp;
	depthAttachmentInfo.stencilLoadOp = loadOp;
	depthAttachmentInfo.initialLayout = ImageLayout::UNDEFINED;
	depthAttachmentInfo.finalLayout = ImageLayout::DEPTH_STENCIL;

	return DepthAttachment::Create(depthAttachmentInfo, pTex);
}

static inline bool NeedToRender(const RenderingData& data)
{
	if (data.GetVertexCount() == 0)
		return false;
	return true;
}