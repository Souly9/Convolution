#pragma once
#include "BackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Attachment.h"

class AttachmentBaseVulkan : public AttachmentBase
{
public:
    AttachmentBaseVulkan()
    {
    }

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

    LoadOp GetLoadOp() const
    {
        return m_loadOp;
    }

    StoreOp GetStoreOp() const
    {
        return m_storeOp;
    }

    ImageLayout GetRenderingLayout() const
    {
        return m_renderingLayout;
    }

    VkClearValue GetClearValue() const
    {
        return m_clearValue;
    }

    void SetClearValue(const mathstl::Vector4& clearValue);

protected:
    AttachmentBaseVulkan(const VkAttachmentDescription& attachmentDesc,
                         Texture* pTexture,
                         TexFormat format,
                         LoadOp loadOp,
                         StoreOp storeOp,
                         ImageLayout renderingLayout);

    VkAttachmentDescription m_attachmentDesc{};
    TexFormat m_format{TexFormat::UNDEFINED};
    // This is the texture that this attachment is attached to, if any
    Texture* m_pTexture{nullptr};
    LoadOp m_loadOp{LoadOp::DONT_CARE};
    StoreOp m_storeOp{StoreOp::DONT_CARE};
    ImageLayout m_renderingLayout{ImageLayout::UNDEFINED};
    VkClearValue m_clearValue{g_BlackCLearColor};
};

class ColorAttachmentVulkan : public AttachmentBaseVulkan, public ColorAttachmentBase
{
public:
    static ColorAttachmentVulkan Create(const ColorAttachmentInfo& createInfo, Texture* pTexture = nullptr);

protected:
    ColorAttachmentVulkan(const VkAttachmentDescription& attachmentDesc,
                          Texture* pTexture,
                          TexFormat format,
                          LoadOp loadOp,
                          StoreOp storeOp,
                          ImageLayout renderingLayout);
};

class DepthAttachmentVulkan : public AttachmentBaseVulkan, public DepthAttachmentBase
{
public:
    DepthAttachmentVulkan()
    {
    }

    static DepthAttachmentVulkan Create(const DepthBufferAttachmentInfo& createInfo, Texture* pTexture = nullptr);

protected:
    DepthAttachmentVulkan(const VkAttachmentDescription& attachmentDesc,
                          Texture* pTexture,
                          TexFormat format,
                          LoadOp loadOp,
                          StoreOp storeOp,
                          ImageLayout renderingLayout);
};
