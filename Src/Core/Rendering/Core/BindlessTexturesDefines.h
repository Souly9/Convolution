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
    GlobalSampledTextures,
    GlobalSamplers,
    GlobalMatrices,
    Custom // Just indicate the class itself will specify all binding slots and so on
};

static inline stltype::hash_map<BindlessType, u32> s_BindlessTypeToSlot = {
    {BindlessType::GlobalTextures, s_globalBindlessTextureBufferBindingSlot},
    {BindlessType::GlobalArrayTextures, s_globalBindlessArrayTextureBufferBindingSlot},
    {BindlessType::GlobalImages, s_globalBindlessImageBufferBindingSlot},
    {BindlessType::GlobalSampledTextures, s_globalBindlessSampledTextureBindingSlot},
    {BindlessType::GlobalSamplers, s_globalSamplerBindingSlot},
    {BindlessType::GlobalMatrices, s_globalBindlessViewMatricesBufferBindingSlot}};
// Descriptor count per bindless binding, owned by the Renderer (g_renderer.GetBindlessCapacity)
u32 GetCount(BindlessType type);

static inline DescriptorType ToDescriptorType(BindlessType type)
{
    switch (type)
    {
        case BindlessType::GlobalTextures:
            return DescriptorType::BindlessTextures;
        case BindlessType::GlobalArrayTextures:
            return DescriptorType::BindlessTextures;
        case BindlessType::GlobalImages:
            return DescriptorType::BindlessImages;
        case BindlessType::GlobalSampledTextures:
            return DescriptorType::BindlessSampledImages;
        case BindlessType::GlobalSamplers:
            return DescriptorType::Samplers;
        case BindlessType::GlobalMatrices:
            return DescriptorType::UniformBuffer;
        default:
            DEBUG_ASSERT(false);
    }

    return DescriptorType::BindlessTextures;
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
        case DescriptorType::BindlessTextures:
            return true;
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
