#pragma once
#include "RenderTraitsMacros.h"

#include "Core/Global/Utils/EnumHelpers.h"
#include "Core/Rendering/LayerDefines.h"
#include "RenderDefinitions.h"
#include "Resource.h"

struct TextureInfoBase
{
    DirectX::XMUINT3 extents;
    ImageLayout layout{ImageLayout::UNDEFINED};
    u64 size{0};
    Usage usage{Usage::Sampled};
    u32 mipLevels;
};

struct TextureInfo : TextureInfoBase
{
    TexFormat format;
};

// Base class for agnostic data and interfaces
class TextureBase : public TrackedResource
{
public:
    TextureBase() = default;
    TextureBase(const TextureInfo& info) : m_info{info}
    {
    }

    const TextureInfo& GetInfo() const
    {
        return m_info;
    }
    TextureInfo& GetInfo()
    {
        return m_info;
    }

protected:
    TextureInfo m_info;
};

#include "APITraits.h"
#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VulkanTraits.h"
#include "Core/Rendering/Vulkan/VkTexture.h"
#elif defined(USE_METAL)
#include "Core/Rendering/Metal/MetalTraits.h"
#include "Core/Rendering/Metal/MtlTexture.h"
#endif

template <typename API>
class TextureT : public APITraits<API>::TextureType
{
public:
    // Inherit constructors
    using APITraits<API>::TextureType::TextureType;
    DECLARE_RENDER_RESOURCE_TRAITS(TextureT, TextureType)
};