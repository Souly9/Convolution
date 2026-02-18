#pragma once
#include "Core/Global/Typedefs.h"
#include "Core/Global/Utils/EnumHelpers.h"

// Base enum types used by BindlessTexturesDefines.h and UBODefines.h
// This file has no dependencies on other descriptor files to break circular includes

enum class ShaderTypeBits : u32
{
    Vertex = 0x00000001,
    Geometry = 0x00000010,
    Fragment = 0x00000100,
    Compute = 0x00001000,
    All = 0x00010000
};
MAKE_FLAG_ENUM(ShaderTypeBits)

enum class DescriptorType
{
    UniformBuffer,
    StorageBuffer,
    CombinedImageSampler,
    BindlessTextures
};
