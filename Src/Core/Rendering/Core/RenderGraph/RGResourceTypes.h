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

#include "Core/Global/Utils/MathFunctions.h"

enum class RGResourceSpecFlags : u32
{
    None          = 0,
    IsPingPong    = 1u << 0,
    NeedsBindless = 1u << 1,
    IsPersistent  = 1u << 2,
    IsBuffer      = 1u << 3
};

struct RGResourceSpec
{
    RGResourceID id{RGResourceID::Custom};
    stltype::string customName{};

    TexFormat format{TexFormat::UNDEFINED};
    RGSizeClass sizeClass{RGSizeClass::RenderResolution};
    mathstl::Vector2 scale{1.0f, 1.0f};
    mathstl::Vector2 fixedExtents{1.0f, 1.0f};
    Usage usage{Usage::Sampled};
    TextureFilter minFilter{TextureFilter::NEAREST};
    TextureFilter magFilter{TextureFilter::NEAREST};
    
    u32 flags{(u32)RGResourceSpecFlags::NeedsBindless};
    u64 bufferSize{0};

    bool IsPingPong() const { return mathstl::isFlagSet(flags, (u32)RGResourceSpecFlags::IsPingPong); }
    void SetIsPingPong(bool v = true) { mathstl::setFlag(flags, (u32)RGResourceSpecFlags::IsPingPong, v); }

    bool NeedsBindless() const { return mathstl::isFlagSet(flags, (u32)RGResourceSpecFlags::NeedsBindless); }
    void SetNeedsBindless(bool v = true) { mathstl::setFlag(flags, (u32)RGResourceSpecFlags::NeedsBindless, v); }

    bool IsPersistent() const { return mathstl::isFlagSet(flags, (u32)RGResourceSpecFlags::IsPersistent); }
    void SetIsPersistent(bool v = true) { mathstl::setFlag(flags, (u32)RGResourceSpecFlags::IsPersistent, v); }

    bool IsBuffer() const { return mathstl::isFlagSet(flags, (u32)RGResourceSpecFlags::IsBuffer); }
    void SetIsBuffer(bool v = true) { mathstl::setFlag(flags, (u32)RGResourceSpecFlags::IsBuffer, v); }

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
