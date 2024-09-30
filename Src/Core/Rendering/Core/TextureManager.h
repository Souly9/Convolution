#pragma once
#include "RenderingTypeDefs.h"

class TextureMan
{
public:
	Tex& CreateTexture(const TextureInfo& name);
	Tex& CreateTexture(const TextureInfo& name, const stltype::string& path);

protected:
	stltype::vector<Texture> m_textures;
};

