#pragma once
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
	ImageLayout renderingLayout = ImageLayout::COLOR_ATTACHMENT;
};
struct DepthBufferAttachmentInfo
{
	TexFormat format;
	LoadOp loadOp = LoadOp::CLEAR;
	StoreOp storeOp = StoreOp::STORE;
	u32 samples = 1u;
	LoadOp stencilLoadOp = LoadOp::IDC;
	StoreOp stencilStoreOp = StoreOp::IDC;
	ImageLayout initialLayout = ImageLayout::UNDEFINED;
	ImageLayout finalLayout = ImageLayout::DEPTH_STENCIL;
	ImageLayout renderingLayout = ImageLayout::DEPTH_STENCIL;
};
