#pragma once
#include "BackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/Attachment.h"

class AttachmentBaseVulkan : public AttachmentBase
{
public:
    AttachmentBaseVulkan() = default;
    AttachmentBaseVulkan(const VkAttachmentDescription& attachmentDesc,
                         Texture* pTexture,
                         TexFormat format,
                         LoadOp loadOp,
                         StoreOp storeOp,
                         ImageLayout renderingLayout);

    TexFormat GetFormat() const
    {
        return m_format;
    }

    const VkAttachmentDescription& GetDesc() const;
    VkAttachmentDescription& GetDescMutable()
    {
        return m_attachmentDesc;
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

    void SetClearValue(const mathstl::Vector4& clearValue);

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
    VkAttachmentDescription m_attachmentDesc{};
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
    VkClearValue m_clearValue{};
};

class ColorAttachmentVulkan : public AttachmentBaseVulkan
{
public:
    ColorAttachmentVulkan() = default;
    ColorAttachmentVulkan(const VkAttachmentDescription& attachmentDesc,
                         Texture* pTexture,
                         TexFormat format,
                         LoadOp loadOp,
                         StoreOp storeOp,
                         ImageLayout renderingLayout);

    static ColorAttachmentVulkan Create(const ColorAttachmentInfo& createInfo, Texture* pTexture = nullptr);
};

class DepthAttachmentVulkan : public AttachmentBaseVulkan
{
public:
    DepthAttachmentVulkan() = default;
    DepthAttachmentVulkan(const VkAttachmentDescription& attachmentDesc,
                         Texture* pTexture,
                         TexFormat format,
                         LoadOp loadOp,
                         StoreOp storeOp,
                         ImageLayout renderingLayout);

    static DepthAttachmentVulkan Create(const DepthBufferAttachmentInfo& createInfo, Texture* pTexture = nullptr);
};
