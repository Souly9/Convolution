#pragma once

struct CascadedShadowMap
{
	u32 cascades;
	TexFormat format;
	TextureHandle handle;
	BindlessTextureHandle bindlessHandle;
	Texture* pTexture;
};