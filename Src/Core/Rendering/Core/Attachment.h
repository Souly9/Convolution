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
class AttachmentBase
{
};

class ColorAttachment : public AttachmentBase
{
};


