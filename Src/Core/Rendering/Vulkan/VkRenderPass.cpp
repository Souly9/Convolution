#include "VkGlobals.h"
#include "VkRenderPass.h"

RPass<Vulkan> RPass<Vulkan>::CreateFullScreenRenderPassSimple(const RenderPassAttachment& colorAttachment)
{
	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; 
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef; 
	
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment.GetHandle();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;

	RPass<Vulkan> renderPass;
	DEBUG_ASSERT(vkCreateRenderPass(VkGlobals::GetLogicalDevice(), &renderPassInfo, VulkanAllocator(), &renderPass.m_renderPass) == VK_SUCCESS);
	return renderPass;
}

VkRenderPass RPass<Vulkan>::GetHandle() const
{
	return m_renderPass;
}

RPass<Vulkan>::~RPass()
{
	vkDestroyRenderPass(VkGlobals::GetLogicalDevice(), m_renderPass, VulkanAllocator());
}