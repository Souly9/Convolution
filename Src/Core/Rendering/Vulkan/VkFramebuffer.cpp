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

FrameBufferVulkan::FrameBufferVulkan(const TextureVulkan* attachmentDesc, const RenderPassVulkan& renderPass, const DirectX::XMUINT3& extents)
{
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = renderPass.GetRef();
	stltype::vector<VkImageView> attachments{ attachmentDesc->GetImageView() };
	framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	framebufferInfo.pAttachments = attachments.data();
	framebufferInfo.width = extents.x;
	framebufferInfo.height = extents.y;
	framebufferInfo.layers = 1;

	CreateVkBuffer(framebufferInfo);

	m_info.extents = extents;
	m_info.format = attachmentDesc->GetInfo().format;
}

FrameBufferVulkan::~FrameBufferVulkan()
{
	TRACKED_DESC_IMPL
}
