#include "Core/Rendering/Core/TextureManager.h"
#include "VkFramebuffer.h"
#include "VkGlobals.h"
#include "VkTexture.h"

const VkFramebuffer& FrameBufferVulkan::GetRef() const
{
	return m_framebuffer;
}

void FrameBufferVulkan::CleanUp()
{
	VK_FREE_IF(m_framebuffer, vkDestroyFramebuffer(VkGlobals::GetLogicalDevice(), m_framebuffer, VulkanAllocator()))
}

void FrameBufferVulkan::CreateVkBuffer(const VkFramebufferCreateInfo& createInfo)
{
	DEBUG_ASSERT(vkCreateFramebuffer(VkGlobals::GetLogicalDevice(), &createInfo, nullptr, &m_framebuffer) == VK_SUCCESS);
}

FrameBufferVulkan::FrameBufferVulkan(const stltype::vector<const TextureVulkan*>& attachments, const RenderPassVulkan& renderPass, const DirectX::XMUINT3& extents) : FrameBufferVulkan(attachments, renderPass.GetRef(), extents)
{
}

FrameBufferVulkan::FrameBufferVulkan(const stltype::vector<const TextureVulkan*>& attachments, const VkRenderPass& renderPass, const DirectX::XMUINT3& extents)
{
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = renderPass;

	stltype::vector<VkImageView> attachmentViews(attachments.size());
	for (size_t i = 0; i < attachments.size(); ++i)
	{
		attachmentViews[i] = attachments[i]->GetImageView();
	}

	framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
	framebufferInfo.pAttachments = attachmentViews.data();
	framebufferInfo.width = extents.x;
	framebufferInfo.height = extents.y;
	framebufferInfo.layers = 1;

	CreateVkBuffer(framebufferInfo);

	m_extents = extents;
}

FrameBufferVulkan::~FrameBufferVulkan()
{
	TRACKED_DESC_IMPL
}
