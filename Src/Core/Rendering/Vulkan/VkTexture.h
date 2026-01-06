#pragma once
#include "BackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Texture.h"

class TextureVulkan : public Tex
{
public:
    friend class TextureMan;
    friend class VkTextureManager;

    TextureVulkan();
    TextureVulkan(const VkImageCreateInfo&, const TextureInfo&);
    TextureVulkan(const TextureInfo&);

    ~TextureVulkan();
    virtual void CleanUp() override;

    void SetImageView(VkImageView view);
    void SetSampler(VkSampler sampler);

    VkImageView GetImageView() const
    {
        return m_imageView;
    }
    VkImage GetImage() const
    {
        return m_image;
    }
    VkSampler GetSampler() const
    {
        return m_sampler;
    }

    virtual void NamingCallBack(const stltype::string& name) override;

protected:
    VkImage m_image{VK_NULL_HANDLE};
    GPUMemoryHandle m_imageMemory{VK_NULL_HANDLE};
    VkSampler m_sampler{VK_NULL_HANDLE};
    VkImageView m_imageView{VK_NULL_HANDLE};
};
