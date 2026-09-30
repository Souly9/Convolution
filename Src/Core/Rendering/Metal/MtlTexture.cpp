#include "MtlTexture.h"

// TODO(Metal): implement with MTL::TextureDescriptor / newTextureView

TextureMetal::TextureMetal()
{
}

TextureMetal::TextureMetal(const TextureInfo& info) : TextureBase(info)
{
}

TextureMetal::~TextureMetal()
{
    TRACKED_DESC_IMPL
}

void TextureMetal::CleanUp()
{
}

void TextureMetal::SetTextureView2D(MTL::Texture* view2D)
{
    m_textureView2D = view2D;
}

void TextureMetal::SetSampler(MTL::SamplerState* sampler)
{
    m_sampler = sampler;
}

void TextureMetal::NamingCallBack(const stltype::string& name)
{
}
