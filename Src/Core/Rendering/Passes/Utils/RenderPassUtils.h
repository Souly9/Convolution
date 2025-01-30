#pragma once
#include "Core/Rendering/Core/Defines/VertexDefines.h"

static inline MinVertex ConvertVertexFormat(const CompleteVertex& completeVert)
{
	return { completeVert.position };
}

struct WriteableRTAttachments
{
	ColorAttachmentVulkan color;
	DepthBufferAttachmentVulkan depth;
};
static inline WriteableRTAttachments CreateWriteableRTAttachments(const RenderPasses::RendererAttachmentInfo& attachmentInfo)
{
	ColorAttachmentInfo colorAttachmentInfo{};
	colorAttachmentInfo.format = attachmentInfo.swapchainTextures[0].GetInfo().format;
	colorAttachmentInfo.loadOp = LoadOp::LOAD;
	colorAttachmentInfo.initialLayout = ImageLayout::COLOR_ATTACHMENT;
	colorAttachmentInfo.finalLayout = ImageLayout::COLOR_ATTACHMENT;
	auto colorAttachment = RenderPassAttachmentColor::Create(colorAttachmentInfo);

	DepthBufferAttachmentInfo depthAttachmentInfo{};
	depthAttachmentInfo.format = DEPTH_BUFFER_FORMAT;
	depthAttachmentInfo.loadOp = LoadOp::LOAD;
	depthAttachmentInfo.stencilLoadOp = LoadOp::LOAD;
	depthAttachmentInfo.initialLayout = ImageLayout::DEPTH_STENCIL;
	auto depthAttachment = DepthBufferAttachmentVulkan::Create(depthAttachmentInfo);

	return { colorAttachment, depthAttachment };
}
static inline WriteableRTAttachments CreateWriteableAndPresentableRT(const RenderPasses::RendererAttachmentInfo& attachmentInfo)
{
	ColorAttachmentInfo colorAttachmentInfo{};
	colorAttachmentInfo.format = attachmentInfo.swapchainTextures[0].GetInfo().format;
	colorAttachmentInfo.loadOp = LoadOp::LOAD;
	colorAttachmentInfo.initialLayout = ImageLayout::COLOR_ATTACHMENT;
	colorAttachmentInfo.finalLayout = ImageLayout::PRESENT;
	auto colorAttachment = RenderPassAttachmentColor::Create(colorAttachmentInfo);

	DepthBufferAttachmentInfo depthAttachmentInfo{};
	depthAttachmentInfo.format = DEPTH_BUFFER_FORMAT;
	depthAttachmentInfo.loadOp = LoadOp::LOAD;
	depthAttachmentInfo.stencilLoadOp = LoadOp::LOAD;
	depthAttachmentInfo.initialLayout = ImageLayout::DEPTH_STENCIL;
	auto depthAttachment = DepthBufferAttachmentVulkan::Create(depthAttachmentInfo);

	return { colorAttachment, depthAttachment };
}

static inline bool NeedToRender(const RenderPass& renderPass)
{
	const auto vertCount = renderPass.GetVertCount();
	if (vertCount == 0)
		return false;
	return true;
}