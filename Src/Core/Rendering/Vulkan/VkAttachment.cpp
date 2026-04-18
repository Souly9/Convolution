#include "VkAttachment.h"
#include "Core/Global/GlobalDefines.h"
#include "Utils/VkEnumHelpers.h"
#include "VkGlobals.h"

void AttachmentBaseVulkan::SetClearValue(const mathstl::Vector4& clearValue)
{
    m_clearValue = {clearValue.x, clearValue.y, clearValue.z, clearValue.w};
}

AttachmentBaseVulkan::AttachmentBaseVulkan(const VkAttachmentDescription& attachmentDesc,
                                           Texture* pTexture,
                                           TexFormat format,
                                           LoadOp loadOp,
                                           StoreOp storeOp,
                                           ImageLayout renderingLayout)
    : m_attachmentDesc(attachmentDesc),
      m_format(format),
      m_pTexture(pTexture),
      m_loadOp(loadOp),
      m_storeOp(storeOp),
      m_renderingLayout(renderingLayout)
{
}

ColorAttachmentVulkan::ColorAttachmentVulkan(const VkAttachmentDescription& attachmentDesc,
                                             Texture* pTexture,
                                             TexFormat format,
                                             LoadOp loadOp,
                                             StoreOp storeOp,
                                             ImageLayout renderingLayout)
    : AttachmentBaseVulkan(attachmentDesc, pTexture, format, loadOp, storeOp, renderingLayout)
{
}

DepthAttachmentVulkan::DepthAttachmentVulkan(const VkAttachmentDescription& attachmentDesc,
                                             Texture* pTexture,
                                             TexFormat format,
                                             LoadOp loadOp,
                                             StoreOp storeOp,
                                             ImageLayout renderingLayout)
    : AttachmentBaseVulkan(attachmentDesc, pTexture, format, loadOp, storeOp, renderingLayout)
{
}

const VkAttachmentDescription& AttachmentBaseVulkan::GetDesc() const
{
    return m_attachmentDesc;
}

ColorAttachmentVulkan ColorAttachmentVulkan::Create(const ColorAttachmentInfo& createInfo, Texture* pTexture)
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = Conv(createInfo.format);
    colorAttachment.samples = Conv(createInfo.samples);
    colorAttachment.loadOp = Conv(createInfo.loadOp);
    colorAttachment.storeOp = Conv(createInfo.storeOp);
    colorAttachment.stencilLoadOp = Conv(createInfo.stencilLoadOp);
    colorAttachment.stencilStoreOp = Conv(createInfo.stencilStoreOp);
    colorAttachment.initialLayout = Conv(createInfo.initialLayout);
    colorAttachment.finalLayout = Conv(createInfo.finalLayout);

    return ColorAttachmentVulkan(colorAttachment,
                                 pTexture,
                                 createInfo.format,
                                 createInfo.loadOp,
                                 createInfo.storeOp,
                                 createInfo.renderingLayout);
}

DepthAttachmentVulkan DepthAttachmentVulkan::Create(const DepthBufferAttachmentInfo& createInfo, Texture* pTexture)
{
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = Conv(createInfo.format);
    depthAttachment.samples = Conv(createInfo.samples);
    depthAttachment.loadOp = Conv(createInfo.loadOp);
    depthAttachment.storeOp = Conv(createInfo.storeOp);
    depthAttachment.stencilLoadOp = Conv(createInfo.stencilLoadOp);
    depthAttachment.stencilStoreOp = Conv(createInfo.stencilStoreOp);
    depthAttachment.initialLayout = Conv(createInfo.initialLayout);
    depthAttachment.finalLayout = Conv(createInfo.finalLayout);

    auto att = DepthAttachmentVulkan(depthAttachment,
                                     pTexture,
                                     createInfo.format,
                                     createInfo.loadOp,
                                     createInfo.storeOp,
                                     createInfo.renderingLayout);
    att.SetClearValue(mathstl::Vector4(1.0f, 0.0f, 0.0f, 0.0f));
    return att;
}
