#pragma once
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "RGResourceID.h"

using RGResourceHandle = u32;
constexpr RGResourceHandle kInvalidRGHandle = UINT32_MAX;

enum class RGSizeClass : u8
{
    RenderResolution,
    OutputResolution,
    Fixed
};

struct RGResourceSpec
{
    RGResourceID id{RGResourceID::Custom};
    stltype::string customName{};

    TexFormat format{TexFormat::UNDEFINED};
    RGSizeClass sizeClass{RGSizeClass::RenderResolution};
    mathstl::Vector2 fixedExtents{1.0f, 1.0f};
    Usage usage{Usage::Sampled};
    TextureFilter minFilter{TextureFilter::NEAREST};
    TextureFilter magFilter{TextureFilter::NEAREST};
    bool isPingPong{false};
    bool needsBindless{true};

    u64 bufferSize{0};
    bool isBuffer{false};

    const char* GetName() const
    {
        return id == RGResourceID::Custom ? customName.c_str() : ToString(id);
    }

    bool MatchesIdentity(const RGResourceSpec& other) const
    {
        if (id != RGResourceID::Custom && other.id != RGResourceID::Custom)
        {
            return id == other.id;
        }
        return customName == other.customName;
    }
};
