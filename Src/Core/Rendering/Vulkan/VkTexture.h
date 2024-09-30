#pragma once
#include "Core/Global/GlobalDefines.h"
#include "BackendDefines.h"
#include "Core/Rendering/Core/Texture.h"

class TextureVulkan : public Tex
{
public:
	friend class TextureMan;
	friend class VkTextureManager;

	TextureVulkan();
	TextureVulkan(const TextureInfo&) = delete;
	TextureVulkan(const VkImageViewCreateInfo&, const TextureInfo&);
	

	VkImageView GetImageView() const { return m_imageView; }
	const TextureInfo& GetInfo() const { return m_info; }

	void SetDebugName(const stltype::string& name);
protected:

	VkImageView m_imageView;
};

