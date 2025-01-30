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
    depthAttachment.samples = Conv(createInfo.samples);
    depthAttachment.loadOp = Conv(createInfo.loadOp);
    depthAttachment.storeOp = Conv(createInfo.storeOp);
    depthAttachment.stencilLoadOp = Conv(createInfo.stencilLoadOp);
    depthAttachment.stencilStoreOp = Conv(createInfo.stencilStoreOp);
    depthAttachment.initialLayout = Conv(createInfo.initialLayout);
    depthAttachment.finalLayout = Conv(createInfo.finalLayout);
    return { depthAttachment };
}

DepthBufferAttachmentVulkan::DepthBufferAttachmentVulkan(const VkAttachmentDescription& attachmentDesc) : AttachmentBaseVulkan(attachmentDesc)
{
}
