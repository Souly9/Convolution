#include "VkTexture.h"
#include "VkGlobals.h"

TexImpl<Vulkan>::TexImpl(const VkImageView& info) : m_imageView(info)
{
}

TexImpl<Vulkan>::~TexImpl()
{
	if (m_imageView)
	{
		vkDestroyImageView(VkGlobals::GetLogicalDevice(), m_imageView, nullptr);
	}
}

TexImpl<Vulkan> TexImpl<Vulkan>::CreateFromImage(const TextureCreationInfoVulkanImage& info)
{
	VkImageViewCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = info.image;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = info.format;
	createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	SetNoMipMap(createInfo);
	SetNoSwizzle(createInfo);
	return { CreateImageView(createInfo) };
}

VkImageView TexImpl<Vulkan>::CreateImageView(const VkImageViewCreateInfo& createInfo)
{
	VkImageView imageView{};
	DEBUG_ASSERT(vkCreateImageView(VkGlobals::GetLogicalDevice(), &createInfo, VulkanAllocator(), &imageView) == VK_SUCCESS);
	return imageView;
}

void TexImpl<Vulkan>::SetNoMipMap(VkImageViewCreateInfo& createInfo)
{
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = 1;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;
}

void TexImpl<Vulkan>::SetMipMap(VkImageViewCreateInfo& createInfo)
{
}

void TexImpl<Vulkan>::SetNoSwizzle(VkImageViewCreateInfo& createInfo)
{
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
}
