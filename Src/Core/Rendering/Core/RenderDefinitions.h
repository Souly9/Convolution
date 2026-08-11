#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Utils/EnumHelpers.h"

// API-Agnostic definitions to decouple Core from Vulkan headers
enum class QueueType
{
    Transfer,
    Compute,
    Graphics
};

enum class PassStage : u32
{
    EarlyCompute = 0,
    PreProcess,
    MainGeometry,
    Lighting,
    PostProcess,
    UI
};

enum class TextureWrapMode
{
    REPEAT,
    MIRRORED_REPEAT,
    CLAMP_TO_EDGE,
    CLAMP_TO_BORDER
};

enum class TextureFilter
{
    NEAREST,
    LINEAR
};

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
    R10G10B10A2_UNORM,
    R10G10B10A2_UINT,
    R11G11B10_FLOAT,
    RGB9E5_FLOAT,
    // Compressed formats
    BC1_RGB_UNORM,
    BC1_RGB_SRGB,
    BC1_RGBA_UNORM,
    BC1_RGBA_SRGB,
    BC2_UNORM,
    BC2_SRGB,
    BC3_UNORM,
    BC3_SRGB,
    BC4_UNORM,
    BC4_SNORM,
    BC5_UNORM,
    BC5_SNORM,
    BC6H_UFLOAT,
    BC6H_SFLOAT,
    BC7_UNORM,
    BC7_SRGB,
    // Depth formats
    D16_UNORM,
    X8_D24_UNORM_PACK32,
    D32_SFLOAT,
    S8_UINT,
    D16_UNORM_S8_UINT,
    D24_UNORM_S8_UINT,
    D32_SFLOAT_S8_UINT
};

inline const char* ToString(TexFormat format)
{
    switch (format)
    {
        case TexFormat::UNDEFINED: return "UNDEFINED";
        case TexFormat::R8_UNORM: return "R8_UNORM";
        case TexFormat::R8_SNORM: return "R8_SNORM";
        case TexFormat::R8_UINT: return "R8_UINT";
        case TexFormat::R8_SINT: return "R8_SINT";
        case TexFormat::R8G8_UNORM: return "R8G8_UNORM";
        case TexFormat::R8G8_SNORM: return "R8G8_SNORM";
        case TexFormat::R8G8_UINT: return "R8G8_UINT";
        case TexFormat::R8G8_SINT: return "R8G8_SINT";
        case TexFormat::R8G8B8_UNORM: return "R8G8B8_UNORM";
        case TexFormat::R8G8B8_SNORM: return "R8G8B8_SNORM";
        case TexFormat::R8G8B8_UINT: return "R8G8B8_UINT";
        case TexFormat::R8G8B8_SINT: return "R8G8B8_SINT";
        case TexFormat::B8G8R8_UNORM: return "B8G8R8_UNORM";
        case TexFormat::B8G8R8_SNORM: return "B8G8R8_SNORM";
        case TexFormat::B8G8R8_UINT: return "B8G8R8_UINT";
        case TexFormat::B8G8R8_SINT: return "B8G8R8_SINT";
        case TexFormat::R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
        case TexFormat::R8G8B8A8_SNORM: return "R8G8B8A8_SNORM";
        case TexFormat::R8G8B8A8_UINT: return "R8G8B8A8_UINT";
        case TexFormat::R8G8B8A8_SINT: return "R8G8B8A8_SINT";
        case TexFormat::B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
        case TexFormat::B8G8R8A8_SNORM: return "B8G8R8A8_SNORM";
        case TexFormat::B8G8R8A8_UINT: return "B8G8R8A8_UINT";
        case TexFormat::B8G8R8A8_SINT: return "B8G8R8A8_SINT";
        case TexFormat::R8G8B8A8_SRGB: return "R8G8B8A8_SRGB";
        case TexFormat::B8G8R8A8_SRGB: return "B8G8R8A8_SRGB";
        case TexFormat::R16_UNORM: return "R16_UNORM";
        case TexFormat::R16_SNORM: return "R16_SNORM";
        case TexFormat::R16_UINT: return "R16_UINT";
        case TexFormat::R16_SINT: return "R16_SINT";
        case TexFormat::R16_FLOAT: return "R16_FLOAT";
        case TexFormat::R16G16_UNORM: return "R16G16_UNORM";
        case TexFormat::R16G16_SNORM: return "R16G16_SNORM";
        case TexFormat::R16G16_UINT: return "R16G16_UINT";
        case TexFormat::R16G16_SINT: return "R16G16_SINT";
        case TexFormat::R16G16_FLOAT: return "R16G16_FLOAT";
        case TexFormat::R16G16B16_UNORM: return "R16G16B16_UNORM";
        case TexFormat::R16G16B16_SNORM: return "R16G16B16_SNORM";
        case TexFormat::R16G16B16_UINT: return "R16G16B16_UINT";
        case TexFormat::R16G16B16_SINT: return "R16G16B16_SINT";
        case TexFormat::R16G16B16_FLOAT: return "R16G16B16_FLOAT";
        case TexFormat::R16G16B16A16_UNORM: return "R16G16B16A16_UNORM";
        case TexFormat::R16G16B16A16_SNORM: return "R16G16B16A16_SNORM";
        case TexFormat::R16G16B16A16_UINT: return "R16G16B16A16_UINT";
        case TexFormat::R16G16B16A16_SINT: return "R16G16B16A16_SINT";
        case TexFormat::R16G16B16A16_FLOAT: return "R16G16B16A16_FLOAT";
        case TexFormat::R32_UINT: return "R32_UINT";
        case TexFormat::R32_SINT: return "R32_SINT";
        case TexFormat::R32_FLOAT: return "R32_FLOAT";
        case TexFormat::R32G32_UINT: return "R32G32_UINT";
        case TexFormat::R32G32_SINT: return "R32G32_SINT";
        case TexFormat::R32G32_FLOAT: return "R32G32_FLOAT";
        case TexFormat::R32G32B32_UINT: return "R32G32B32_UINT";
        case TexFormat::R32G32B32_SINT: return "R32G32B32_SINT";
        case TexFormat::R32G32B32_FLOAT: return "R32G32B32_FLOAT";
        case TexFormat::R32G32B32A32_UINT: return "R32G32B32A32_UINT";
        case TexFormat::R32G32B32A32_SINT: return "R32G32B32A32_SINT";
        case TexFormat::R32G32B32A32_FLOAT: return "R32G32B32A32_FLOAT";
        case TexFormat::R10G10B10A2_UNORM: return "R10G10B10A2_UNORM";
        case TexFormat::R10G10B10A2_UINT: return "R10G10B10A2_UINT";
        case TexFormat::R11G11B10_FLOAT: return "R11G11B10_FLOAT";
        case TexFormat::RGB9E5_FLOAT: return "RGB9E5_FLOAT";
        case TexFormat::BC1_RGB_UNORM: return "BC1_RGB_UNORM";
        case TexFormat::BC1_RGB_SRGB: return "BC1_RGB_SRGB";
        case TexFormat::BC1_RGBA_UNORM: return "BC1_RGBA_UNORM";
        case TexFormat::BC1_RGBA_SRGB: return "BC1_RGBA_SRGB";
        case TexFormat::BC2_UNORM: return "BC2_UNORM";
        case TexFormat::BC2_SRGB: return "BC2_SRGB";
        case TexFormat::BC3_UNORM: return "BC3_UNORM";
        case TexFormat::BC3_SRGB: return "BC3_SRGB";
        case TexFormat::BC4_UNORM: return "BC4_UNORM";
        case TexFormat::BC4_SNORM: return "BC4_SNORM";
        case TexFormat::BC5_UNORM: return "BC5_UNORM";
        case TexFormat::BC5_SNORM: return "BC5_SNORM";
        case TexFormat::BC6H_UFLOAT: return "BC6H_UFLOAT";
        case TexFormat::BC6H_SFLOAT: return "BC6H_SFLOAT";
        case TexFormat::BC7_UNORM: return "BC7_UNORM";
        case TexFormat::BC7_SRGB: return "BC7_SRGB";
        case TexFormat::D16_UNORM: return "D16_UNORM";
        case TexFormat::X8_D24_UNORM_PACK32: return "X8_D24_UNORM_PACK32";
        case TexFormat::D32_SFLOAT: return "D32_SFLOAT";
        case TexFormat::S8_UINT: return "S8_UINT";
        case TexFormat::D16_UNORM_S8_UINT: return "D16_UNORM_S8_UINT";
        case TexFormat::D24_UNORM_S8_UINT: return "D24_UNORM_S8_UINT";
        case TexFormat::D32_SFLOAT_S8_UINT: return "D32_SFLOAT_S8_UINT";
        default: return "Unknown";
    }
}

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
    DONT_CARE,
    NONE
};

enum class DepthCompareOp
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

inline constexpr bool kUseReversedZDepth = true;
inline constexpr f32 kDepthClearValue = kUseReversedZDepth ? 0.0f : 1.0f;
inline constexpr DepthCompareOp kDepthWriteCompareOp =
    kUseReversedZDepth ? DepthCompareOp::GREATER_OR_EQUAL : DepthCompareOp::LESS_OR_EQUAL;

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
