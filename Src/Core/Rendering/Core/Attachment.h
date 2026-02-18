#pragma once
#include "Texture.h"

struct ColorAttachmentInfo
{
    TexFormat format;
    LoadOp loadOp = LoadOp::CLEAR;
    StoreOp storeOp = StoreOp::STORE;
    u32 samples = 1u;
    LoadOp stencilLoadOp = LoadOp::DONT_CARE;
    StoreOp stencilStoreOp = StoreOp::DONT_CARE;
    ImageLayout initialLayout = ImageLayout::UNDEFINED;
    ImageLayout finalLayout = ImageLayout::PRESENT_SRC_KHR;
    ImageLayout renderingLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
};
struct DepthBufferAttachmentInfo
{
    TexFormat format;
    LoadOp loadOp = LoadOp::CLEAR;
    StoreOp storeOp = StoreOp::STORE;
    u32 samples = 1u;
    LoadOp stencilLoadOp = LoadOp::DONT_CARE;
    StoreOp stencilStoreOp = StoreOp::DONT_CARE;
    ImageLayout initialLayout = ImageLayout::UNDEFINED;
    ImageLayout finalLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    ImageLayout renderingLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
};
