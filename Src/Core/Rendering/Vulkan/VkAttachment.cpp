#include "VkAttachment.h"
#include "VkRenderPass.h"
#include "Utils/VkEnumHelpers.h"

AttachmentBaseVulkan::AttachmentBaseVulkan(const VkAttachmentDescription& attachmentDesc) : m_attachmentDesc(attachmentDesc)
{
}

ColorAttachmentVulkan::ColorAttachmentVulkan(const VkAttachmentDescription& attachmentDesc) : AttachmentBaseVulkan(attachmentDesc)
{
}

const VkAttachmentDescription& AttachmentBaseVulkan::GetDesc() const
{
	return m_attachmentDesc;
}

ColorAttachmentVulkan ColorAttachmentVulkan::CreateColorAttachment(const ColorAttachmentInfo& createInfo)
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

	return { colorAttachment };
}