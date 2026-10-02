#pragma once
#include "BackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Texture.h"

class TextureMan;
class VkTextureManager;

class TextureVulkan : public TextureBase
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
    void SetImageView2D(VkImageView view2D);

    VkImageView GetImageView() const
    {
        return m_imageView;
    }
    VkImageView GetImageView2D() const
    {
        return m_imageView2D != VK_NULL_HANDLE ? m_imageView2D : m_imageView;
    }
    VkImage GetImage() const
    {
        return m_image;
    }

    virtual void NamingCallBack(const stltype::string& name) override;

protected:
    VkImage m_image{VK_NULL_HANDLE};
    GPUMemoryHandle m_imageMemory{VK_NULL_HANDLE};
    VkImageView m_imageView{VK_NULL_HANDLE};
    VkImageView m_imageView2D{VK_NULL_HANDLE};
};
