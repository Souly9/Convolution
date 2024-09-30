#include "VkGlobals.h"
#include "VkTexture.h"

TextureVulkan::TextureVulkan()
{
	SetDebugName("Invalid");
}

TextureVulkan::TextureVulkan(const VkImageViewCreateInfo& createInfo, const TextureInfo& info) : Tex(info)
{
	DEBUG_ASSERT(vkCreateImageView(VkGlobals::GetLogicalDevice(), &createInfo, VulkanAllocator(), &m_imageView) == VK_SUCCESS);
}

void TextureVulkan::SetDebugName(const stltype::string& name)
{
#ifdef CONV_DEBUG
	m_debugString = name;
#endif
}

