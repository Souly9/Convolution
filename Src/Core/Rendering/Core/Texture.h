#pragma once
#include "Core/Global/GlobalDefines.h"
GLOBAL_INCLUDES

enum TexFormat : u32
{
	R8G8B8A8_UNORM = 0,

	SWAPCHAIN = 999
};

enum class ImageLayout
{
	UNDEFINED,
	TRANSFER_SRC_OPTIMAL,
	TRANSFER_DST_OPTIMAL,
	PRESENT,
};

enum class LoadOp
{
	CLEAR,
	LOAD,
	IDC
};

enum class StoreOp
{
	STORE,
	IDC
};

struct TextureCreationInfoBase
{
	u32 width = 0;
	u32 height = 0;
};

struct TextureCreationInfo : TextureCreationInfoBase
{
	TexFormat format = TexFormat::R8G8B8A8_UNORM;
};

IMPLEMENT_GRAPHICS_API
class TexImpl
{
public:
	static TexImpl CreateFromFile() {}
	static TexImpl CreateFromMemory(const void* data, const TextureCreationInfo& info);

private:
	TexImpl<BackendAPI> m_implementation;
};

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkTexture.h"
#endif

using Texture = TexImpl<RenderAPI>;