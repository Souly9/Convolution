#pragma once
#include "BackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Attachment.h"

class AttachmentBaseVulkan
{
public:
    AttachmentBaseVulkan()
    {
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

    VkImageLayout GetRenderingLayout() const
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
                         VkImageLayout renderingLayout);

    VkAttachmentDescription m_attachmentDesc{};
    // This is the texture that this attachment is attached to, if any
    Texture* m_pTexture{nullptr};
    // Layout used for rendering, not pretty but needed for dynamic rendering and it's good enough
    VkImageLayout m_renderingLayout{VK_IMAGE_LAYOUT_UNDEFINED};
    VkClearValue m_clearValue{g_BlackCLearColor};
};

class ColorAttachmentVulkan : public AttachmentBaseVulkan
{
public:
    static ColorAttachmentVulkan Create(const ColorAttachmentInfo& createInfo, Texture* pTexture = nullptr);

protected:
    ColorAttachmentVulkan(const VkAttachmentDescription& attachmentDesc,
                          Texture* pTexture,
                          VkImageLayout renderingLayout);
};

class DepthBufferAttachmentVulkan : public AttachmentBaseVulkan
{
public:
    DepthBufferAttachmentVulkan()
    {
    }

    static DepthBufferAttachmentVulkan Create(const DepthBufferAttachmentInfo& createInfo, Texture* pTexture = nullptr);

protected:
    DepthBufferAttachmentVulkan(const VkAttachmentDescription& attachmentDesc,
                                Texture* pTexture,
                                VkImageLayout renderingLayout);
};
