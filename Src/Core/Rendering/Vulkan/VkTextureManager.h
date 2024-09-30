#pragma once
#include "Core/Global/GlobalDefines.h"
#include "BackendDefines.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "VkTexture.h"

enum VkFormat;

struct TextureCreationInfoVulkan : public TextureInfoBase
{
	VkFormat format;
};

struct TextureCreationInfoVulkanImage
{
	VkFormat format;
	VkImage image;
};

class VkTextureManager : public TextureMan
{
public:
	VkTextureManager();
	TextureVulkan CreateFromImage(const TextureCreationInfoVulkanImage& info, const TextureInfoBase& infoBase);

	~VkTextureManager();

protected:

	static void SetNoMipMap(VkImageViewCreateInfo& createInfo);
	static void SetMipMap(VkImageViewCreateInfo& createInfo);
	static void SetNoSwizzle(VkImageViewCreateInfo& createInfo);
};
