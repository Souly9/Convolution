#include "VkGlobals.h"
#include "VkTextureManager.h"

VkTextureManager::VkTextureManager()
{
	m_textures.reserve(512);
}

TextureVulkan VkTextureManager::CreateFromImage(const TextureCreationInfoVulkanImage& info, const TextureInfoBase& infoBase)
{

	VkImageViewCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	createInfo.image = info.image;
	createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	createInfo.format = info.format;
	createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	SetNoMipMap(createInfo);
	SetNoSwizzle(createInfo);

	TextureInfo genericInfo{};
	genericInfo.format = info.format;
	genericInfo.extents = infoBase.extents;
	auto& tex = m_textures.emplace_back(createInfo, genericInfo);

	return tex;
}

VkTextureManager::~VkTextureManager()
{
	for(auto& t : m_textures)
	{
		vkDestroyImageView(VkGlobals::GetLogicalDevice(), t.GetImageView(), nullptr);
	}
}

void VkTextureManager::SetNoMipMap(VkImageViewCreateInfo& createInfo)
{
	createInfo.subresourceRange.baseMipLevel = 0;
	createInfo.subresourceRange.levelCount = 1;
	createInfo.subresourceRange.baseArrayLayer = 0;
	createInfo.subresourceRange.layerCount = 1;
}

void VkTextureManager::SetMipMap(VkImageViewCreateInfo& createInfo)
{
}

void VkTextureManager::SetNoSwizzle(VkImageViewCreateInfo& createInfo)
{
	createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
}