#pragma once
#include "Core/Rendering/Core/RenderingIncludes.h"
#include "Core/Rendering/Core/Defines/VertexDefines.h"
#include <cstring>

struct WriteableRTAttachments
{
    ColorAttachment color;
    DepthAttachment depth;
};

static inline RenderAttachmentInfo ToRenderAttachmentInfo(const ColorAttachment& colorAttachment)
{
    RenderAttachmentInfo info{};
    info.pTexture = const_cast<Texture*>(colorAttachment.GetTexture());
    info.renderingLayout = colorAttachment.GetRenderingLayout();
    info.loadOp = colorAttachment.GetLoadOp();
    info.storeOp = colorAttachment.GetStoreOp();
    return info;
}

static inline RenderAttachmentInfo ToRenderAttachmentInfo(const DepthAttachment& depthAttachment)
{
    RenderAttachmentInfo info{};
    info.pTexture = const_cast<Texture*>(depthAttachment.GetTexture());
    info.renderingLayout = depthAttachment.GetRenderingLayout();
    info.loadOp = depthAttachment.GetLoadOp();
    info.storeOp = depthAttachment.GetStoreOp();
    return info;
}

static inline stltype::vector<RenderAttachmentInfo> ToRenderAttachmentInfos(const stltype::vector<ColorAttachment>& colorAttachments)
{
    stltype::vector<RenderAttachmentInfo> result;
    result.reserve(colorAttachments.size());
    for (const auto& colorAttachment : colorAttachments)
    {
        result.push_back(ToRenderAttachmentInfo(colorAttachment));
    }
    return result;
}

// Helper to create a default ColorAttachment with optional finalLayout
static inline ColorAttachment CreateDefaultColorAttachment(TexFormat format, LoadOp loadOp, ImageLayout finalLayout, Texture* pTex)
{
    ColorAttachmentInfo info{};
    info.format = format;
    info.loadOp = loadOp;
    info.storeOp = StoreOp::STORE;
    info.initialLayout = ImageLayout::UNDEFINED;
    info.finalLayout = finalLayout;
    info.renderingLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    
    auto att = ColorAttachment::Create(info, pTex);
    att.SetClearValue(mathstl::Vector4(0.0f, 0.0f, 0.0f, 1.0f));
    return att;
}

static inline ColorAttachment CreateDefaultColorAttachment(TexFormat format, LoadOp loadOp, Texture* pTex)
{
    return CreateDefaultColorAttachment(format, loadOp, ImageLayout::PRESENT_SRC_KHR, pTex);
}

static inline DepthAttachment CreateDefaultDepthAttachment(LoadOp loadOp, StoreOp storeOp, Texture* pTex)
{
    DepthBufferAttachmentInfo info{};
    info.format = pTex ? (TexFormat)pTex->GetInfo().format : DEPTH_BUFFER_FORMAT;

    info.loadOp = loadOp;
    info.storeOp = storeOp;
    info.initialLayout = ImageLayout::UNDEFINED;
    info.finalLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    info.renderingLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    
    auto att = DepthAttachment::Create(info, pTex);
    att.SetClearValue(mathstl::Vector4(kDepthClearValue, 0.0f, 0.0f, 0.0f));
    return att;
}

static inline DepthAttachment CreateDefaultDepthAttachment(LoadOp loadOp, Texture* pTex)
{
    return CreateDefaultDepthAttachment(loadOp, StoreOp::STORE, pTex);
}

static inline DepthAttachment CreateReadOnlyDepthAttachment(LoadOp loadOp, Texture* pTex)
{
    DepthBufferAttachmentInfo info{};
    info.format = pTex ? (TexFormat)pTex->GetInfo().format : DEPTH_BUFFER_FORMAT;

    info.loadOp = loadOp;
    info.storeOp = StoreOp::NONE;
    info.initialLayout = ImageLayout::UNDEFINED;
    info.finalLayout = ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    info.renderingLayout = ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    auto att = DepthAttachment::Create(info, pTex);
    att.SetClearValue(mathstl::Vector4(kDepthClearValue, 0.0f, 0.0f, 0.0f));
    return att;
}

static inline bool NeedToRender(const IndirectDrawCmdBuf& buffer)
{
    if (buffer.GetDrawCmdNum() == 0)
        return false;
    return true;
}
