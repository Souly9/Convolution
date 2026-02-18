#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Utils/EnumHelpers.h"

// API-Agnostic definitions to decouple Core from Vulkan headers
enum class TexFormat
{
    UNDEFINED,
    R8_UNORM,
    R8_SNORM,
    R8_UINT,
    R8_SINT,
    R8G8_UNORM,
    R8G8_SNORM,
    R8G8_UINT,
    R8G8_SINT,
    R8G8B8_UNORM,
    R8G8B8_SNORM,
    R8G8B8_UINT,
    R8G8B8_SINT,
    B8G8R8_UNORM,
    B8G8R8_SNORM,
    B8G8R8_UINT,
    B8G8R8_SINT,
    R8G8B8A8_UNORM,
    R8G8B8A8_SNORM,
    R8G8B8A8_UINT,
    R8G8B8A8_SINT,
    B8G8R8A8_UNORM,
    B8G8R8A8_SNORM,
    B8G8R8A8_UINT,
    B8G8R8A8_SINT,
    R8G8B8A8_SRGB,
    B8G8R8A8_SRGB,
    R16_UNORM,
    R16_SNORM,
    R16_UINT,
    R16_SINT,
    R16_FLOAT,
    R16G16_UNORM,
    R16G16_SNORM,
    R16G16_UINT,
    R16G16_SINT,
    R16G16_FLOAT,
    R16G16B16_UNORM,
    R16G16B16_SNORM,
    R16G16B16_UINT,
    R16G16B16_SINT,
    R16G16B16_FLOAT,
    R16G16B16A16_UNORM,
    R16G16B16A16_SNORM,
    R16G16B16A16_UINT,
    R16G16B16A16_SINT,
    R16G16B16A16_FLOAT,
    R32_UINT,
    R32_SINT,
    R32_FLOAT,
    R32G32_UINT,
    R32G32_SINT,
    R32G32_FLOAT,
    R32G32B32_UINT,
    R32G32B32_SINT,
    R32G32B32_FLOAT,
    R32G32B32A32_UINT,
    R32G32B32A32_SINT,
    R32G32B32A32_FLOAT,
    // Depth formats
    D16_UNORM,
    X8_D24_UNORM_PACK32,
    D32_SFLOAT,
    S8_UINT,
    D16_UNORM_S8_UINT,
    D24_UNORM_S8_UINT,
    D32_SFLOAT_S8_UINT
};

enum class ImageLayout
{
    UNDEFINED,
    GENERAL,
    COLOR_ATTACHMENT_OPTIMAL,
    DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    DEPTH_STENCIL_READ_ONLY_OPTIMAL,
    SHADER_READ_ONLY_OPTIMAL,
    TRANSFER_SRC_OPTIMAL,
    TRANSFER_DST_OPTIMAL,
    PREINITIALIZED,
    PRESENT_SRC_KHR
};

enum class LoadOp
{
    LOAD,
    CLEAR,
    DONT_CARE
};

enum class StoreOp
{
    STORE,
    DONT_CARE
};

enum class CompareOp
{
    NEVER,
    LESS,
    EQUAL,
    LESS_OR_EQUAL,
    GREATER,
    NOT_EQUAL,
    GREATER_OR_EQUAL,
    ALWAYS
};

enum class CullMode
{
    NONE,
    FRONT,
    BACK,
    FRONT_AND_BACK
};

enum class FrontFace
{
    COUNTER_CLOCKWISE,
    CLOCKWISE
};

// Add more as needed...

union ClearColorValue
{
    f32 float32[4];
    s32 int32[4];
    u32 uint32[4];
};

struct ClearDepthStencilValue
{
    f32 depth;
    u32 stencil;
};

enum class Usage
{
    None = 0,
    GBuffer = 1 << 0,
    ColorAttachment = 1 << 1,
    DepthAttachment = 1 << 2,
    TransferSrc = 1 << 3,
    TransferDst = 1 << 4,
    Sampled = 1 << 5,
    Storage = 1 << 6,
    AttachmentReadWrite = 1 << 7,
    StencilAttachment = 1 << 8,
    ShadowMap = 1 << 9,
    TransientAttachment = 1 << 10,
    InputAttachment = 1 << 11,
    DepthStencilAttachment = DepthAttachment | StencilAttachment
};
MAKE_FLAG_ENUM(Usage)

enum class Tiling
{
    OPTIMAL,
    LINEAR
};

union ClearValue
{
    ClearColorValue color;
    ClearDepthStencilValue depthStencil;
};
