#include "Core/Global/GlobalDefines.h"
#include "VkAttachment.h"
#include "Utils/VkEnumHelpers.h"
#include "VkGlobals.h"

void AttachmentBaseVulkan::SetClearValue(const mathstl::Vector4& clearValue)
{
	m_clearValue = { clearValue.x, clearValue.y, clearValue.z, clearValue.w };
}

AttachmentBaseVulkan::AttachmentBaseVulkan(const VkAttachmentDescription& attachmentDesc, Texture* pTexture, VkImageLayout renderingLayout) :
    m_attachmentDesc(attachmentDesc), m_pTexture(pTexture), m_renderingLayout(renderingLayout)
{
}

ColorAttachmentVulkan::ColorAttachmentVulkan(const VkAttachmentDescription& attachmentDesc, Texture* pTexture, VkImageLayout renderingLayout) : 
    AttachmentBaseVulkan(attachmentDesc, pTexture, renderingLayout)
{
}

DepthBufferAttachmentVulkan::DepthBufferAttachmentVulkan(const VkAttachmentDescription& attachmentDesc, Texture* pTexture, VkImageLayout renderingLayout) : 
    AttachmentBaseVulkan(attachmentDesc, pTexture, renderingLayout)
{
}

const VkAttachmentDescription& AttachmentBaseVulkan::GetDesc() const
{
	return m_attachmentDesc;
}

ColorAttachmentVulkan ColorAttachmentVulkan::Create(const ColorAttachmentInfo& createInfo, Texture* pTexture)
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = createInfo.format;
    colorAttachment.samples = Conv(createInfo.samples);
    colorAttachment.loadOp = Conv(createInfo.loadOp);
    colorAttachment.storeOp = Conv(createInfo.storeOp);
    colorAttachment.stencilLoadOp = Conv(createInfo.stencilLoadOp);
    colorAttachment.stencilStoreOp = Conv(createInfo.stencilStoreOp);
    colorAttachment.initialLayout = Conv(createInfo.initialLayout);
    colorAttachment.finalLayout = Conv(createInfo.finalLayout);

	return { colorAttachment, pTexture, Conv(createInfo.renderingLayout) };
}

DepthBufferAttachmentVulkan DepthBufferAttachmentVulkan::Create(const DepthBufferAttachmentInfo& createInfo, Texture* pTexture)
{
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = createInfo.format;
    depthAttachment.samples = Conv(createInfo.samples);
    depthAttachment.loadOp = Conv(createInfo.loadOp);
    depthAttachment.storeOp = Conv(createInfo.storeOp);
    depthAttachment.stencilLoadOp = Conv(createInfo.stencilLoadOp);
    depthAttachment.stencilStoreOp = Conv(createInfo.stencilStoreOp);
    depthAttachment.initialLayout = Conv(createInfo.initialLayout);
    depthAttachment.finalLayout = Conv(createInfo.finalLayout);

    return { depthAttachment, pTexture, Conv(createInfo.renderingLayout) };
}
