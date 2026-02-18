#pragma once
#include "../PassManager.h"
#include "Core/Rendering/Core/Defines/VertexDefines.h"
#include "Core/Rendering/Core/CommandBuffer.h"

struct WriteableRTAttachments
{
    ColorAttachmentVulkan color;
    DepthBufferAttachmentVulkan depth;
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
    att.SetClearValue(mathstl::Vector4(1.0f, 0.0f, 0.0f, 0.0f)); // Depth 1.0, Stencil 0
    return att;
}

static inline DepthAttachment CreateDefaultDepthAttachment(LoadOp loadOp, Texture* pTex)
{
    return CreateDefaultDepthAttachment(loadOp, StoreOp::STORE, pTex);
}

static inline bool NeedToRender(const IndirectDrawCmdBuf& buffer)
{
    if (buffer.GetDrawCmdNum() == 0)
        return false;
    return true;
}

// Helper to convert ColorAttachment to RenderAttachmentInfo
static inline RenderAttachmentInfo ToRenderAttachmentInfo(const ColorAttachment& attachment)
{
    RenderAttachmentInfo info{};
    info.pTexture = const_cast<Texture*>(attachment.GetTexture());
    // Use the tracked layout if available, otherwise assume optimal
    if (info.pTexture)
    {
        // Ideally we query the texture for its layout, but Attachment wrapper holds a rendering layout
        info.renderingLayout = (ImageLayout)attachment.GetRenderingLayout(); 
    }
    else
    {
        info.renderingLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    }
    
    info.loadOp = (LoadOp)attachment.GetDesc().loadOp;
    info.storeOp = (StoreOp)attachment.GetDesc().storeOp;
    
    auto clear = attachment.GetClearValue();
    memcpy(&info.clearValue, &clear, sizeof(VkClearValue));
    
    return info;
}

static inline RenderAttachmentInfo ToRenderAttachmentInfo(const DepthAttachment& attachment)
{
    RenderAttachmentInfo info{};
    info.pTexture = const_cast<Texture*>(attachment.GetTexture());
    info.renderingLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    info.loadOp = (LoadOp)attachment.GetDesc().loadOp;
    info.storeOp = (StoreOp)attachment.GetDesc().storeOp;
    
    auto clear = attachment.GetClearValue();
    memcpy(&info.clearValue, &clear, sizeof(VkClearValue));
    
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
