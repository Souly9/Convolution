#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Utils/MemoryUtilities.h"
#include "Texture.h"

struct ColorAttachmentInfo
{
	TexFormat format;
	LoadOp loadOp = LoadOp::CLEAR;
	StoreOp storeOp = StoreOp::STORE;
	u32 samples = 1u;
	LoadOp stencilLoadOp = LoadOp::IDC;
	StoreOp stencilStoreOp = StoreOp::IDC;
	ImageLayout initialLayout = ImageLayout::UNDEFINED;
	ImageLayout finalLayout = ImageLayout::PRESENT;
};

// Generic attachment for render passes
IMPLEMENT_GRAPHICS_API
class Attachment
{
};

IMPLEMENT_GRAPHICS_API
class ColorAttachment : public Attachment<BackendAPI>
{
};


using RenderPassAttachment = Attachment<RenderAPI>;
using RenderPassAttachmentColor = ColorAttachment<RenderAPI>;

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkAttachment.h"
#endif
