#pragma once
#include "Core/Rendering/Core/RenderingIncludes.h"
#include "Core/Rendering/Core/Defines/VertexDefines.h"
#include <cstring>

struct WriteableRTAttachments
{
    ColorAttachment color;
    DepthAttachment depth;
};
// Helper to create a default ColorAttachment
static inline ColorAttachment CreateDefaultColorAttachment(TexFormat format, LoadOp loadOp, Texture* pTex)
{
    ColorAttachmentInfo info{};
    info.format = format;
    info.loadOp = loadOp;
    info.storeOp = StoreOp::STORE;
    info.initialLayout = ImageLayout::UNDEFINED;
    info.finalLayout = ImageLayout::PRESENT_SRC_KHR;
    info.renderingLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    
    auto att = ColorAttachment::Create(info, pTex);
    att.SetClearValue(mathstl::Vector4(0.0f, 0.0f, 0.0f, 1.0f));
    return att;
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

static inline RenderAttachmentInfo ToRenderAttachmentInfo(const ColorAttachment& attachment)
{
    RenderAttachmentInfo info{};
    info.pTexture = const_cast<Texture*>(attachment.GetTexture());
    if (info.pTexture)
    {
        info.renderingLayout = attachment.GetRenderingLayout();
    }
    else
    {
        info.renderingLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    }
    
    info.loadOp = attachment.GetLoadOp();
    info.storeOp = attachment.GetStoreOp();
    
    auto clear = attachment.GetClearValue();
    memcpy(&info.clearValue, &clear, sizeof(clear));
    
    return info;
}

static inline RenderAttachmentInfo ToRenderAttachmentInfo(const DepthAttachment& attachment)
{
    RenderAttachmentInfo info{};
    info.pTexture = const_cast<Texture*>(attachment.GetTexture());
    info.renderingLayout = attachment.GetRenderingLayout();
    info.loadOp = attachment.GetLoadOp();
    info.storeOp = attachment.GetStoreOp();
    
    auto clear = attachment.GetClearValue();
    memcpy(&info.clearValue, &clear, sizeof(clear));
    
    return info;
}

static inline stltype::vector<RenderAttachmentInfo> ToRenderAttachmentInfos(const stltype::vector<ColorAttachment>& attachments)
{
    stltype::vector<RenderAttachmentInfo> infos;
    infos.reserve(attachments.size());
    for (const auto& att : attachments)
    {
        infos.push_back(ToRenderAttachmentInfo(att));
    }
    return infos;
}
