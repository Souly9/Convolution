#include "Core/Global/GlobalDefines.h"
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

ColorAttachmentVulkan ColorAttachmentVulkan::Create(const ColorAttachmentInfo& createInfo)
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

DepthBufferAttachmentVulkan DepthBufferAttachmentVulkan::Create(const DepthBufferAttachmentInfo& createInfo)
{
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = createInfo.format;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    return { depthAttachment };
}

DepthBufferAttachmentVulkan::DepthBufferAttachmentVulkan(const VkAttachmentDescription& attachmentDesc) : AttachmentBaseVulkan(attachmentDesc)
{
}
