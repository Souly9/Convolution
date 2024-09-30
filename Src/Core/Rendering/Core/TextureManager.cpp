#include "Core/Global/GlobalDefines.h"
#include "TextureManager.h"

Tex& TextureMan::CreateTexture(const TextureInfo& name)
{
	auto& t = m_textures.emplace_back();
	return t;
}
