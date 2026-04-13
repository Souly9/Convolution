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

enum class TextureStatus : u8
{
    Loading,
    Created,
    Ready,
    Failed
};

// Base class for agnostic data and interfaces
class TextureBase : public TrackedResource
{
public:
    TextureBase() = default;
    TextureBase(const TextureInfo& info) : m_info{info}
    {
    }

    virtual void CleanUp() override
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
    void SetStatus(TextureStatus status)
    {
        m_status = status;
    }

    TextureStatus GetStatus() const
    {
        return m_status;
    }

protected:
    TextureInfo m_info;
    TextureStatus m_status{TextureStatus::Loading};
};


#include "APITraits.h"
#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VulkanTraits.h"
#include "Core/Rendering/Vulkan/VkTexture.h"
#endif

template <typename API>
class TextureT : public APITraits<API>::TextureType
{
public:
    // Inherit constructors
    using APITraits<API>::TextureType::TextureType;
    DECLARE_RENDER_RESOURCE_TRAITS(TextureT, TextureType)
};