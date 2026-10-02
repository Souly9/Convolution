#include "MtlTextureManager.h"

// TODO(Metal): port VkTextureManager request/upload flow (blit encoder uploads, bindless argument buffer)

MtlTextureManager::MtlTextureManager()
{
}

MtlTextureManager::~MtlTextureManager()
{
}

void MtlTextureManager::Init()
{
}

void MtlTextureManager::CreateSwapchainTextures(const TextureInfoBase& infoBase)
{
}

void MtlTextureManager::CreateTexture(const FileTextureRequest& fileReq)
{
}

Texture* MtlTextureManager::CreateTextureImmediate(const DynamicTextureRequest& req)
{
    return nullptr;
}

void MtlTextureManager::WriteBindlessTexture(Texture* pTex, u32 slot)
{
}

void MtlTextureManager::DestroyTextureDeferred(stltype::unique_ptr<Texture> pTexture)
{
}

// TODO(Metal): MTL::Texture::newTextureView and the ImGui Metal backend
TextureViewHandle MtlTextureManager::CreateDepthLayerView(const Texture& texture, TexFormat format, u32 layer)
{
    return nullptr;
}

void MtlTextureManager::DestroyTextureView(TextureViewHandle view)
{
}

u64 MtlTextureManager::RegisterImGuiTexture(const Texture& texture)
{
    return 0;
}

u64 MtlTextureManager::RegisterImGuiTextureView(TextureViewHandle view)
{
    return 0;
}

void MtlTextureManager::UnregisterImGuiTexture(u64 id)
{
}
