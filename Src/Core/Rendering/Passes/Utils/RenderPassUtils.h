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

static inline WriteableRTAttachments CreateWriteableRTAttachments(RenderPasses::RendererAttachmentInfo& attachmentInfo, bool clearAttachments = false)
{
	ColorAttachmentInfo colorAttachmentInfo{};
	colorAttachmentInfo.format = attachmentInfo.swapchainTextures[0].GetInfo().format;
	colorAttachmentInfo.loadOp = clearAttachments ? LoadOp::CLEAR : LoadOp::LOAD;
	colorAttachmentInfo.storeOp = StoreOp::STORE;
	colorAttachmentInfo.initialLayout = ImageLayout::COLOR_ATTACHMENT;
	colorAttachmentInfo.finalLayout = ImageLayout::COLOR_ATTACHMENT;
	auto colorAttachment = ColorAttachment::Create(colorAttachmentInfo, attachmentInfo.colorAttachments[RenderPasses::ColorAttachmentType::GBufferColor][0].GetTexture());

	DepthBufferAttachmentInfo depthAttachmentInfo{};
	depthAttachmentInfo.format = DEPTH_BUFFER_FORMAT;
	depthAttachmentInfo.loadOp = clearAttachments ? LoadOp::CLEAR : LoadOp::LOAD;
	depthAttachmentInfo.stencilLoadOp = clearAttachments ? LoadOp::CLEAR : LoadOp::LOAD;
	depthAttachmentInfo.initialLayout = ImageLayout::DEPTH_STENCIL;
	depthAttachmentInfo.finalLayout = ImageLayout::DEPTH_STENCIL;
	auto depthAttachment = DepthAttachment::Create(depthAttachmentInfo, attachmentInfo.depthAttachment.GetTexture());

	return { colorAttachment, depthAttachment };
}
static inline WriteableRTAttachments CreateWriteableAndPresentableRT(const RenderPasses::RendererAttachmentInfo& attachmentInfo)
{
	ColorAttachmentInfo colorAttachmentInfo{};
	colorAttachmentInfo.format = attachmentInfo.swapchainTextures[0].GetInfo().format;
	colorAttachmentInfo.loadOp = LoadOp::LOAD;
	colorAttachmentInfo.initialLayout = ImageLayout::COLOR_ATTACHMENT;
	colorAttachmentInfo.finalLayout = ImageLayout::PRESENT;
	auto colorAttachment = ColorAttachment::Create(colorAttachmentInfo);

	DepthBufferAttachmentInfo depthAttachmentInfo{};
	depthAttachmentInfo.format = DEPTH_BUFFER_FORMAT;
	depthAttachmentInfo.loadOp = LoadOp::LOAD;
	depthAttachmentInfo.storeOp = StoreOp::STORE;
	depthAttachmentInfo.stencilLoadOp = LoadOp::LOAD;
	depthAttachmentInfo.initialLayout = ImageLayout::DEPTH_STENCIL;
	auto depthAttachment = DepthBufferAttachmentVulkan::Create(depthAttachmentInfo);

	return { colorAttachment, depthAttachment };
}

static inline bool NeedToRender(const RenderingData& data)
{
	if (data.GetVertexCount() == 0)
		return false;
	return true;
}