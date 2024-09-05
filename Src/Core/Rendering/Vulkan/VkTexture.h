#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Texture.h"
#include "BackendDefines.h"

enum VkFormat;

struct TextureCreationInfoVulkan : public TextureCreationInfoBase
{
	VkFormat format;
}; 

struct TextureCreationInfoVulkanImage
{
	VkFormat format;
	VkImage image;
};

template<>
class TexImpl<Vulkan>
{
public:
	TexImpl() = default;
	~TexImpl();

	static TexImpl<Vulkan> CreateFromImage(const TextureCreationInfoVulkanImage& info);

private:
	TexImpl(const VkImageView& info);
	static VkImageView CreateImageView(const VkImageViewCreateInfo& createInfo);

	static void SetNoMipMap(VkImageViewCreateInfo& createInfo);
	static void SetMipMap(VkImageViewCreateInfo& createInfo);
	static void SetNoSwizzle(VkImageViewCreateInfo& createInfo);

	VkImageView m_imageView;
};

using VulkanTex = TexImpl<Vulkan>;
