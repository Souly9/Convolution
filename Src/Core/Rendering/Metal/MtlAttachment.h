#pragma once
#include "MtlBackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/Attachment.h"

// Maps to MTL::RenderPassColor/DepthAttachmentDescriptor at BeginRendering; layouts are ignored on Metal
class AttachmentBaseMetal : public AttachmentBase
{
public:
    AttachmentBaseMetal() = default;
    AttachmentBaseMetal(Texture* pTexture, TexFormat format, LoadOp loadOp, StoreOp storeOp, ImageLayout renderingLayout)
        : m_format(format), m_loadOp(loadOp), m_storeOp(storeOp), m_renderingLayout(renderingLayout),
          m_pTexture(pTexture)
    {
    }

    TexFormat GetFormat() const
    {
        return m_format;
    }

    const Texture* GetTexture() const
    {
        return m_pTexture;
    }
    Texture* GetTexture()
    {
        return m_pTexture;
    }
    void SetTexture(Texture* pTexture)
    {
        m_pTexture = pTexture;
    }

    void SetClearValue(const mathstl::Vector4& clearValue)
    {
        m_clearValue = clearValue;
    }
    const mathstl::Vector4& GetClearValue() const
    {
        return m_clearValue;
    }

    LoadOp GetLoadOp() const
    {
        return m_loadOp;
    }
    StoreOp GetStoreOp() const
    {
        return m_storeOp;
    }
    StoreOp GetStencilStoreOp() const
    {
        return m_stencilStoreOp;
    }
    LoadOp GetStencilLoadOp() const
    {
        return m_stencilLoadOp;
    }
    ImageLayout GetInitialLayout() const
    {
        return m_initialLayout;
    }
    ImageLayout GetFinalLayout() const
    {
        return m_finalLayout;
    }
    ImageLayout GetRenderingLayout() const
    {
        return m_renderingLayout;
    }
    u32 GetSamples() const
    {
        return m_samples;
    }

protected:
    TexFormat m_format{TexFormat::UNDEFINED};
    LoadOp m_loadOp{LoadOp::CLEAR};
    StoreOp m_storeOp{StoreOp::STORE};

    LoadOp m_stencilLoadOp{LoadOp::DONT_CARE};
    StoreOp m_stencilStoreOp{StoreOp::DONT_CARE};

    ImageLayout m_initialLayout{ImageLayout::UNDEFINED};
    ImageLayout m_finalLayout{ImageLayout::PRESENT_SRC_KHR};
    ImageLayout m_renderingLayout{ImageLayout::COLOR_ATTACHMENT_OPTIMAL};
    u32 m_samples{1u};

    Texture* m_pTexture{nullptr};
    mathstl::Vector4 m_clearValue{};
};

class ColorAttachmentMetal : public AttachmentBaseMetal
{
public:
    using AttachmentBaseMetal::AttachmentBaseMetal;

    static ColorAttachmentMetal Create(const ColorAttachmentInfo& createInfo, Texture* pTexture = nullptr)
    {
        ColorAttachmentMetal attachment(
            pTexture, createInfo.format, createInfo.loadOp, createInfo.storeOp, createInfo.renderingLayout);
        attachment.m_samples = createInfo.samples;
        attachment.m_stencilLoadOp = createInfo.stencilLoadOp;
        attachment.m_stencilStoreOp = createInfo.stencilStoreOp;
        attachment.m_initialLayout = createInfo.initialLayout;
        attachment.m_finalLayout = createInfo.finalLayout;
        return attachment;
    }
};

class DepthAttachmentMetal : public AttachmentBaseMetal
{
public:
    using AttachmentBaseMetal::AttachmentBaseMetal;

    static DepthAttachmentMetal Create(const DepthBufferAttachmentInfo& createInfo, Texture* pTexture = nullptr)
    {
        DepthAttachmentMetal attachment(
            pTexture, createInfo.format, createInfo.loadOp, createInfo.storeOp, createInfo.renderingLayout);
        attachment.m_samples = createInfo.samples;
        attachment.m_stencilLoadOp = createInfo.stencilLoadOp;
        attachment.m_stencilStoreOp = createInfo.stencilStoreOp;
        attachment.m_initialLayout = createInfo.initialLayout;
        attachment.m_finalLayout = createInfo.finalLayout;
        return attachment;
    }
};
