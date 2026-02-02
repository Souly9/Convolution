#pragma once
#include "Core/Rendering/Core/RenderingForwardDecls.h"

// Use VkFormat for now - TexFormat was just an alias
using TexFormat = VkFormat;

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