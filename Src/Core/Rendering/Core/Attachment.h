#pragma once
#include "RenderTraitsMacros.h"
#include "RenderDefinitions.h"

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

class AttachmentBase
{
public:
    virtual ~AttachmentBase() = default;
};

class DepthAttachmentBase : public AttachmentBase
{
public:
    virtual ~DepthAttachmentBase() = default;
};

class ColorAttachmentBase : public AttachmentBase
{
public:
    virtual ~ColorAttachmentBase() = default;
};

#include "APITraits.h"
#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkAttachment.h"
#include "Core/Rendering/Vulkan/VulkanTraits.h"
#endif

template <typename API>
class ColorAttachmentT : public APITraits<API>::ColorAttachmentType
{
public:
    using APITraits<API>::ColorAttachmentType::ColorAttachmentType;
    DECLARE_RENDER_RESOURCE_TRAITS(ColorAttachmentT, ColorAttachmentType)
};

template <typename API>
class DepthAttachmentT : public APITraits<API>::DepthAttachmentType
{
public:
    using APITraits<API>::DepthAttachmentType::DepthAttachmentType;
    DECLARE_RENDER_RESOURCE_TRAITS(DepthAttachmentT, DepthAttachmentType)
};
