#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Defines/BindingSlots.h"
#include "Defines/DescriptorEnums.h"

namespace Bindless
{
// Main bindless buffer for all textures of the scene

// Maps UBO template type to its binding slot
enum class BindlessType : u32
{
    GlobalTextures,
    GlobalArrayTextures,
    GlobalImages,
    GlobalSamplers,
    GlobalMatrices,
    Custom // Just indicate the class itself will specify all binding slots and so on
};

static inline stltype::hash_map<BindlessType, u32> s_BindlessTypeToSlot = {
    {BindlessType::GlobalTextures, s_globalBindlessTextureBufferBindingSlot},
    {BindlessType::GlobalArrayTextures, s_globalBindlessArrayTextureBufferBindingSlot},
    {BindlessType::GlobalImages, s_globalBindlessImageBufferBindingSlot},
    {BindlessType::GlobalSamplers, s_globalSamplerBindingSlot},
    {BindlessType::GlobalMatrices, s_globalBindlessViewMatricesBufferBindingSlot}};
// Descriptor count per bindless binding; constant, so layouts can be built before the device exists
static inline u32 GetCount(BindlessType type)
{
    switch (type)
    {
        case BindlessType::GlobalTextures:
        case BindlessType::GlobalArrayTextures:
        case BindlessType::GlobalImages:
            return MAX_BINDLESS_TEXTURES;
        case BindlessType::GlobalSamplers:
            return GLOBAL_SAMPLER_COUNT;
        case BindlessType::GlobalMatrices:
            return 1;
        default:
            DEBUG_ASSERT(false);
            return 0;
    }
}

static inline DescriptorType ToDescriptorType(BindlessType type)
{
    switch (type)
    {
        // Texture-only: combined image samplers each count against the per-stage sampler limit (1024 on MoltenVK)
        case BindlessType::GlobalTextures:
            return DescriptorType::BindlessSampledImages;
        case BindlessType::GlobalArrayTextures:
            return DescriptorType::BindlessSampledImages;
        case BindlessType::GlobalImages:
            return DescriptorType::BindlessImages;
        case BindlessType::GlobalSamplers:
            return DescriptorType::Samplers;
        case BindlessType::GlobalMatrices:
            return DescriptorType::UniformBuffer;
        default:
            DEBUG_ASSERT(false);
    }

    return DescriptorType::BindlessSampledImages;
}

static inline bool IsBindless(DescriptorType type)
{
    switch (type)
    {
        case DescriptorType::UniformBuffer:
            return false;
        case DescriptorType::StorageBuffer:
            return false;
        case DescriptorType::AccelerationStructure:
            return false;
        case DescriptorType::CombinedImageSampler:
            return false;
        case DescriptorType::BindlessImages:
            return true;
        case DescriptorType::BindlessSampledImages:
            return true;
        case DescriptorType::Samplers:
            return false;
        default:
            DEBUG_ASSERT(false);
    }

    return false;
}
} // namespace Bindless
