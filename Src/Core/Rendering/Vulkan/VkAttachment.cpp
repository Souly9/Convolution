#include "VkGlobals.h"
#include "VkRenderPass.h"
#include "VkAttachment.h"

Attachment<Vulkan>::Attachment(const VkAttachmentDescription& attachmentDesc) : m_attachmentDesc(attachmentDesc)
{
}

ColorAttachment<Vulkan>::ColorAttachment(const VkAttachmentDescription& attachmentDesc) : Attachment<Vulkan>(attachmentDesc)
{
}

const VkAttachmentDescription& Attachment<Vulkan>::GetHandle() const
{
	return m_attachmentDesc;
}

ColorAttachment<Vulkan> ColorAttachment<Vulkan>::CreateColorAttachment(const ColorAttachmentInfo& createInfo)
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VkGlobals::GetSwapChainImageFormat();
    colorAttachment.samples = Conv(createInfo.samples);
    colorAttachment.loadOp = Conv(createInfo.loadOp);
    colorAttachment.storeOp = Conv(createInfo.storeOp);
    colorAttachment.stencilLoadOp = Conv(createInfo.stencilLoadOp);
    colorAttachment.stencilStoreOp = Conv(createInfo.stencilStoreOp);
    colorAttachment.initialLayout = Conv(createInfo.initialLayout);
    colorAttachment.finalLayout = Conv(createInfo.finalLayout);

	return { colorAttachment };
}