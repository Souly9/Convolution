#pragma once
#include "Core/Rendering/Core/RenderingForwardDecls.h"

#include "Core/Rendering/Core/RenderDefinitions.h"

// Handle for texture resource
using TextureHandle = u32;

struct CascadedShadowMap
{
    u32 cascades;
    TexFormat format;
    TextureHandle handle;
    BindlessTextureHandle bindlessHandle;
    Texture* pTexture;
    stltype::vector<VkImageView> cascadeViews; // Per-layer 2D views for debug display
};