#pragma once
#include "MtlBackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Texture.h"

class MtlTextureManager;

// MTL::Texture wrapper; Metal has no image layouts, so TextureInfo::layout is bookkeeping only
class TextureMetal : public TextureBase
{
public:
    friend class MtlTextureManager;

    TextureMetal();
    TextureMetal(const TextureInfo&);

    ~TextureMetal();

    virtual void CleanUp() override;

    // Texture views replace VkImageView
    void SetTextureView2D(MTL::Texture* view2D);
    void SetSampler(MTL::SamplerState* sampler);

    MTL::Texture* GetTexture() const
    {
        return m_texture;
    }
    MTL::Texture* GetTextureView2D() const
    {
        return m_textureView2D != nullptr ? m_textureView2D : m_texture;
    }
    MTL::SamplerState* GetSampler() const
    {
        return m_sampler;
    }

    virtual void NamingCallBack(const stltype::string& name) override;

protected:
    MTL::Texture* m_texture{nullptr};
    MTL::Texture* m_textureView2D{nullptr};
    MTL::SamplerState* m_sampler{nullptr};
};
