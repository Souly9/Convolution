#pragma once
#include "Core/Rendering/Core/Buffer.h"
#include "Core/Rendering/Core/Defines/UBODefines.h"
#include "Core/Rendering/Core/Pipeline.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/Texture.h"
#include <vk_mem_alloc.h>

#include "TextureEnums.h"
#include "Core/Rendering/Core/RenderDefinitions.h"

inline VkFormat Conv(const TexFormat& m)
{
    switch (m)
    {
        case TexFormat::UNDEFINED: return VK_FORMAT_UNDEFINED;
        case TexFormat::R8_UNORM: return VK_FORMAT_R8_UNORM;
        case TexFormat::R8_SNORM: return VK_FORMAT_R8_SNORM;
        case TexFormat::R8_UINT: return VK_FORMAT_R8_UINT;
        case TexFormat::R8_SINT: return VK_FORMAT_R8_SINT;
        case TexFormat::R8G8_UNORM: return VK_FORMAT_R8G8_UNORM;
        case TexFormat::R8G8_SNORM: return VK_FORMAT_R8G8_SNORM;
        case TexFormat::R8G8_UINT: return VK_FORMAT_R8G8_UINT;
        case TexFormat::R8G8_SINT: return VK_FORMAT_R8G8_SINT;
        case TexFormat::R8G8B8_UNORM: return VK_FORMAT_R8G8B8_UNORM;
        case TexFormat::R8G8B8_SNORM: return VK_FORMAT_R8G8B8_SNORM;
        case TexFormat::R8G8B8_UINT: return VK_FORMAT_R8G8B8_UINT;
        case TexFormat::R8G8B8_SINT: return VK_FORMAT_R8G8B8_SINT;
        case TexFormat::B8G8R8_UNORM: return VK_FORMAT_B8G8R8_UNORM;
        case TexFormat::B8G8R8_SNORM: return VK_FORMAT_B8G8R8_SNORM;
        case TexFormat::B8G8R8_UINT: return VK_FORMAT_B8G8R8_UINT;
        case TexFormat::B8G8R8_SINT: return VK_FORMAT_B8G8R8_SINT;
        case TexFormat::R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
        case TexFormat::R8G8B8A8_SNORM: return VK_FORMAT_R8G8B8A8_SNORM;
        case TexFormat::R8G8B8A8_UINT: return VK_FORMAT_R8G8B8A8_UINT;
        case TexFormat::R8G8B8A8_SINT: return VK_FORMAT_R8G8B8A8_SINT;
        case TexFormat::B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
        case TexFormat::B8G8R8A8_SNORM: return VK_FORMAT_B8G8R8A8_SNORM;
        case TexFormat::B8G8R8A8_UINT: return VK_FORMAT_B8G8R8A8_UINT;
        case TexFormat::B8G8R8A8_SINT: return VK_FORMAT_B8G8R8A8_SINT;
        case TexFormat::R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case TexFormat::B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
        case TexFormat::R16_UNORM: return VK_FORMAT_R16_UNORM;
        case TexFormat::R16_SNORM: return VK_FORMAT_R16_SNORM;
        case TexFormat::R16_UINT: return VK_FORMAT_R16_UINT;
        case TexFormat::R16_SINT: return VK_FORMAT_R16_SINT;
        case TexFormat::R16_FLOAT: return VK_FORMAT_R16_SFLOAT;
        case TexFormat::R16G16_UNORM: return VK_FORMAT_R16G16_UNORM;
        case TexFormat::R16G16_SNORM: return VK_FORMAT_R16G16_SNORM;
        case TexFormat::R16G16_UINT: return VK_FORMAT_R16G16_UINT;
        case TexFormat::R16G16_SINT: return VK_FORMAT_R16G16_SINT;
        case TexFormat::R16G16_FLOAT: return VK_FORMAT_R16G16_SFLOAT;
        case TexFormat::R16G16B16_UNORM: return VK_FORMAT_R16G16B16_UNORM;
        case TexFormat::R16G16B16_SNORM: return VK_FORMAT_R16G16B16_SNORM;
        case TexFormat::R16G16B16_UINT: return VK_FORMAT_R16G16B16_UINT;
        case TexFormat::R16G16B16_SINT: return VK_FORMAT_R16G16B16_SINT;
        case TexFormat::R16G16B16_FLOAT: return VK_FORMAT_R16G16B16_SFLOAT;
        case TexFormat::R16G16B16A16_UNORM: return VK_FORMAT_R16G16B16A16_UNORM;
        case TexFormat::R16G16B16A16_SNORM: return VK_FORMAT_R16G16B16A16_SNORM;
        case TexFormat::R16G16B16A16_UINT: return VK_FORMAT_R16G16B16A16_UINT;
        case TexFormat::R16G16B16A16_SINT: return VK_FORMAT_R16G16B16A16_SINT;
        case TexFormat::R16G16B16A16_FLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TexFormat::R32_UINT: return VK_FORMAT_R32_UINT;
        case TexFormat::R32_SINT: return VK_FORMAT_R32_SINT;
        case TexFormat::R32_FLOAT: return VK_FORMAT_R32_SFLOAT;
        case TexFormat::R32G32_UINT: return VK_FORMAT_R32G32_UINT;
        case TexFormat::R32G32_SINT: return VK_FORMAT_R32G32_SINT;
        case TexFormat::R32G32_FLOAT: return VK_FORMAT_R32G32_SFLOAT;
        case TexFormat::R32G32B32_UINT: return VK_FORMAT_R32G32B32_UINT;
        case TexFormat::R32G32B32_SINT: return VK_FORMAT_R32G32B32_SINT;
        case TexFormat::R32G32B32_FLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
        case TexFormat::R32G32B32A32_UINT: return VK_FORMAT_R32G32B32A32_UINT;
        case TexFormat::R32G32B32A32_SINT: return VK_FORMAT_R32G32B32A32_SINT;
        case TexFormat::R32G32B32A32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case TexFormat::R10G10B10A2_UNORM: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case TexFormat::R10G10B10A2_UINT: return VK_FORMAT_A2B10G10R10_UINT_PACK32;
        case TexFormat::R11G11B10_FLOAT: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        case TexFormat::RGB9E5_FLOAT: return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
        case TexFormat::BC1_RGB_UNORM: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        case TexFormat::BC1_RGB_SRGB: return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
        case TexFormat::BC1_RGBA_UNORM: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case TexFormat::BC1_RGBA_SRGB: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case TexFormat::BC2_UNORM: return VK_FORMAT_BC2_UNORM_BLOCK;
        case TexFormat::BC2_SRGB: return VK_FORMAT_BC2_SRGB_BLOCK;
        case TexFormat::BC3_UNORM: return VK_FORMAT_BC3_UNORM_BLOCK;
        case TexFormat::BC3_SRGB: return VK_FORMAT_BC3_SRGB_BLOCK;
        case TexFormat::BC4_UNORM: return VK_FORMAT_BC4_UNORM_BLOCK;
        case TexFormat::BC4_SNORM: return VK_FORMAT_BC4_SNORM_BLOCK;
        case TexFormat::BC5_UNORM: return VK_FORMAT_BC5_UNORM_BLOCK;
        case TexFormat::BC5_SNORM: return VK_FORMAT_BC5_SNORM_BLOCK;
        case TexFormat::BC6H_UFLOAT: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case TexFormat::BC6H_SFLOAT: return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case TexFormat::BC7_UNORM: return VK_FORMAT_BC7_UNORM_BLOCK;
        case TexFormat::BC7_SRGB: return VK_FORMAT_BC7_SRGB_BLOCK;
        case TexFormat::D16_UNORM: return VK_FORMAT_D16_UNORM;
        case TexFormat::X8_D24_UNORM_PACK32: return VK_FORMAT_X8_D24_UNORM_PACK32;
        case TexFormat::D32_SFLOAT: return VK_FORMAT_D32_SFLOAT;
        case TexFormat::S8_UINT: return VK_FORMAT_S8_UINT;
        case TexFormat::D16_UNORM_S8_UINT: return VK_FORMAT_D16_UNORM_S8_UINT;
        case TexFormat::D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
        case TexFormat::D32_SFLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
        default: DEBUG_ASSERT(false); return VK_FORMAT_UNDEFINED;
    }
}

inline TexFormat Conv(VkFormat fmt)
{
    switch (fmt)
    {
        case VK_FORMAT_UNDEFINED: return TexFormat::UNDEFINED;
        case VK_FORMAT_R8_UNORM: return TexFormat::R8_UNORM;
        case VK_FORMAT_R8_SNORM: return TexFormat::R8_SNORM;
        case VK_FORMAT_R8_UINT: return TexFormat::R8_UINT;
        case VK_FORMAT_R8_SINT: return TexFormat::R8_SINT;
        case VK_FORMAT_R8G8_UNORM: return TexFormat::R8G8_UNORM;
        case VK_FORMAT_R8G8_SNORM: return TexFormat::R8G8_SNORM;
        case VK_FORMAT_R8G8_UINT: return TexFormat::R8G8_UINT;
        case VK_FORMAT_R8G8_SINT: return TexFormat::R8G8_SINT;
        case VK_FORMAT_R8G8B8_UNORM: return TexFormat::R8G8B8_UNORM;
        case VK_FORMAT_R8G8B8_SNORM: return TexFormat::R8G8B8_SNORM;
        case VK_FORMAT_R8G8B8_UINT: return TexFormat::R8G8B8_UINT;
        case VK_FORMAT_R8G8B8_SINT: return TexFormat::R8G8B8_SINT;
        case VK_FORMAT_B8G8R8_UNORM: return TexFormat::B8G8R8_UNORM;
        case VK_FORMAT_B8G8R8_SNORM: return TexFormat::B8G8R8_SNORM;
        case VK_FORMAT_B8G8R8_UINT: return TexFormat::B8G8R8_UINT;
        case VK_FORMAT_B8G8R8_SINT: return TexFormat::B8G8R8_SINT;
        case VK_FORMAT_R8G8B8A8_UNORM: return TexFormat::R8G8B8A8_UNORM;
        case VK_FORMAT_R8G8B8A8_SNORM: return TexFormat::R8G8B8A8_SNORM;
        case VK_FORMAT_R8G8B8A8_UINT: return TexFormat::R8G8B8A8_UINT;
        case VK_FORMAT_R8G8B8A8_SINT: return TexFormat::R8G8B8A8_SINT;
        case VK_FORMAT_B8G8R8A8_UNORM: return TexFormat::B8G8R8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_SNORM: return TexFormat::B8G8R8A8_SNORM;
        case VK_FORMAT_B8G8R8A8_UINT: return TexFormat::B8G8R8A8_UINT;
        case VK_FORMAT_B8G8R8A8_SINT: return TexFormat::B8G8R8A8_SINT;
        case VK_FORMAT_R8G8B8A8_SRGB: return TexFormat::R8G8B8A8_SRGB;
        case VK_FORMAT_B8G8R8A8_SRGB: return TexFormat::B8G8R8A8_SRGB;
        case VK_FORMAT_R16_UNORM: return TexFormat::R16_UNORM;
        case VK_FORMAT_R16_SNORM: return TexFormat::R16_SNORM;
        case VK_FORMAT_R16_UINT: return TexFormat::R16_UINT;
        case VK_FORMAT_R16_SINT: return TexFormat::R16_SINT;
        case VK_FORMAT_R16_SFLOAT: return TexFormat::R16_FLOAT;
        case VK_FORMAT_R16G16_UNORM: return TexFormat::R16G16_UNORM;
        case VK_FORMAT_R16G16_SNORM: return TexFormat::R16G16_SNORM;
        case VK_FORMAT_R16G16_UINT: return TexFormat::R16G16_UINT;
        case VK_FORMAT_R16G16_SINT: return TexFormat::R16G16_SINT;
        case VK_FORMAT_R16G16_SFLOAT: return TexFormat::R16G16_FLOAT;
        case VK_FORMAT_R16G16B16_UNORM: return TexFormat::R16G16B16_UNORM;
        case VK_FORMAT_R16G16B16_SNORM: return TexFormat::R16G16B16_SNORM;
        case VK_FORMAT_R16G16B16_UINT: return TexFormat::R16G16B16_UINT;
        case VK_FORMAT_R16G16B16_SINT: return TexFormat::R16G16B16_SINT;
        case VK_FORMAT_R16G16B16_SFLOAT: return TexFormat::R16G16B16_FLOAT;
        case VK_FORMAT_R16G16B16A16_UNORM: return TexFormat::R16G16B16A16_UNORM;
        case VK_FORMAT_R16G16B16A16_SNORM: return TexFormat::R16G16B16A16_SNORM;
        case VK_FORMAT_R16G16B16A16_UINT: return TexFormat::R16G16B16A16_UINT;
        case VK_FORMAT_R16G16B16A16_SINT: return TexFormat::R16G16B16A16_SINT;
        case VK_FORMAT_R16G16B16A16_SFLOAT: return TexFormat::R16G16B16A16_FLOAT;
        case VK_FORMAT_R32_UINT: return TexFormat::R32_UINT;
        case VK_FORMAT_R32_SINT: return TexFormat::R32_SINT;
        case VK_FORMAT_R32_SFLOAT: return TexFormat::R32_FLOAT;
        case VK_FORMAT_R32G32_UINT: return TexFormat::R32G32_UINT;
        case VK_FORMAT_R32G32_SINT: return TexFormat::R32G32_SINT;
        case VK_FORMAT_R32G32_SFLOAT: return TexFormat::R32G32_FLOAT;
        case VK_FORMAT_R32G32B32_UINT: return TexFormat::R32G32B32_UINT;
        case VK_FORMAT_R32G32B32_SINT: return TexFormat::R32G32B32_SINT;
        case VK_FORMAT_R32G32B32_SFLOAT: return TexFormat::R32G32B32_FLOAT;
        case VK_FORMAT_R32G32B32A32_UINT: return TexFormat::R32G32B32A32_UINT;
        case VK_FORMAT_R32G32B32A32_SINT: return TexFormat::R32G32B32A32_SINT;
        case VK_FORMAT_R32G32B32A32_SFLOAT: return TexFormat::R32G32B32A32_FLOAT;
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return TexFormat::R10G10B10A2_UNORM;
        case VK_FORMAT_A2B10G10R10_UINT_PACK32: return TexFormat::R10G10B10A2_UINT;
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return TexFormat::R11G11B10_FLOAT;
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return TexFormat::RGB9E5_FLOAT;
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK: return TexFormat::BC1_RGB_UNORM;
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK: return TexFormat::BC1_RGB_SRGB;
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return TexFormat::BC1_RGBA_UNORM;
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return TexFormat::BC1_RGBA_SRGB;
        case VK_FORMAT_BC2_UNORM_BLOCK: return TexFormat::BC2_UNORM;
        case VK_FORMAT_BC2_SRGB_BLOCK: return TexFormat::BC2_SRGB;
        case VK_FORMAT_BC3_UNORM_BLOCK: return TexFormat::BC3_UNORM;
        case VK_FORMAT_BC3_SRGB_BLOCK: return TexFormat::BC3_SRGB;
        case VK_FORMAT_BC4_UNORM_BLOCK: return TexFormat::BC4_UNORM;
        case VK_FORMAT_BC4_SNORM_BLOCK: return TexFormat::BC4_SNORM;
        case VK_FORMAT_BC5_UNORM_BLOCK: return TexFormat::BC5_UNORM;
        case VK_FORMAT_BC5_SNORM_BLOCK: return TexFormat::BC5_SNORM;
        case VK_FORMAT_BC6H_UFLOAT_BLOCK: return TexFormat::BC6H_UFLOAT;
        case VK_FORMAT_BC6H_SFLOAT_BLOCK: return TexFormat::BC6H_SFLOAT;
        case VK_FORMAT_BC7_UNORM_BLOCK: return TexFormat::BC7_UNORM;
        case VK_FORMAT_BC7_SRGB_BLOCK: return TexFormat::BC7_SRGB;
        case VK_FORMAT_D16_UNORM: return TexFormat::D16_UNORM;
        case VK_FORMAT_X8_D24_UNORM_PACK32: return TexFormat::X8_D24_UNORM_PACK32;
        case VK_FORMAT_D32_SFLOAT: return TexFormat::D32_SFLOAT;
        case VK_FORMAT_S8_UINT: return TexFormat::S8_UINT;
        case VK_FORMAT_D16_UNORM_S8_UINT: return TexFormat::D16_UNORM_S8_UINT;
        case VK_FORMAT_D24_UNORM_S8_UINT: return TexFormat::D24_UNORM_S8_UINT;
        case VK_FORMAT_D32_SFLOAT_S8_UINT: return TexFormat::D32_SFLOAT_S8_UINT;
        default: 
            DEBUG_ASSERT(false);
            break;
    }
    return TexFormat::UNDEFINED;
}

inline VkPolygonMode Conv(const PolygonMode& m)
{
    switch (m)
    {
        case (PolygonMode::Fill):
        {
            return VK_POLYGON_MODE_FILL;
        }
        case (PolygonMode::Point):
        {
            return VK_POLYGON_MODE_POINT;
        }
        case (PolygonMode::Line):
        {
            return VK_POLYGON_MODE_LINE;
        }
        default:
        {
            ASSERT(false);
            break;
        }
    }
    return VK_POLYGON_MODE_FILL;
}

inline VkPrimitiveTopology Conv(const Topology& m)
{
    switch (m)
    {
        case Topology::Points:
            return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            break;
        case Topology::Lines:
            return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            break;
        case Topology::LineStrip:
            return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
            break;
        case Topology::TriangleList:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            break;
        case Topology::TriangleStrip:
            return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            break;
    }

    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

inline VkCullModeFlagBits Conv(const CullMode& m)
{
    switch (m)
    {
        case CullMode::NONE:
            return VK_CULL_MODE_NONE;
            break;
        case CullMode::FRONT:
            return VK_CULL_MODE_FRONT_BIT;
            break;
        case CullMode::BACK:
            return VK_CULL_MODE_BACK_BIT;
            break;
        case CullMode::FRONT_AND_BACK:
            return VK_CULL_MODE_FRONT_AND_BACK;
            break;
    }

    return VK_CULL_MODE_FRONT_BIT;
}

inline VkFrontFace Conv(const FrontFace& m)
{
    switch (m)
    {
        case FrontFace::COUNTER_CLOCKWISE:
            return VK_FRONT_FACE_COUNTER_CLOCKWISE;
            break;
        case FrontFace::CLOCKWISE:
            return VK_FRONT_FACE_CLOCKWISE;
            break;
    }

    return VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

inline VkAttachmentLoadOp Conv(const LoadOp& m)
{
    switch (m)
    {
        case LoadOp::LOAD:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
            break;
        case LoadOp::CLEAR:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
            break;
        case LoadOp::DONT_CARE:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            break;
    }
    DEBUG_ASSERT(false);
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

inline VkAttachmentStoreOp Conv(const StoreOp& m)
{
    switch (m)
    {
        case StoreOp::STORE:
            return VK_ATTACHMENT_STORE_OP_STORE;
            break;
        case StoreOp::DONT_CARE:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
            break;
    }

    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

inline VkImageLayout Conv(const ImageLayout& m)
{
    switch (m)
    {
        case ImageLayout::UNDEFINED:
            return VK_IMAGE_LAYOUT_UNDEFINED;
            break;
        case ImageLayout::TRANSFER_SRC_OPTIMAL:
            return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            break;
        case ImageLayout::TRANSFER_DST_OPTIMAL:
            return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            break;
        case ImageLayout::SHADER_READ_ONLY_OPTIMAL:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;
        case ImageLayout::PRESENT_SRC_KHR:
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            break;
        case ImageLayout::COLOR_ATTACHMENT_OPTIMAL:
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        case ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            break;
        case ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            break;    
        case ImageLayout::GENERAL:
            return VK_IMAGE_LAYOUT_GENERAL;
            break;
        case ImageLayout::PREINITIALIZED:
            return VK_IMAGE_LAYOUT_PREINITIALIZED;
            break;
    }
    DEBUG_ASSERT(false);
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

inline VkSampleCountFlagBits Conv(const u32& m)
{
    switch (m)
    {
        case 1:
            return VK_SAMPLE_COUNT_1_BIT;
            break;
        case 2:
            return VK_SAMPLE_COUNT_2_BIT;
            break;
        case 4:
            return VK_SAMPLE_COUNT_4_BIT;
            break;
        case 8:
            return VK_SAMPLE_COUNT_8_BIT;
            break;
        case 16:
            return VK_SAMPLE_COUNT_16_BIT;
            break;
        case 32:
            return VK_SAMPLE_COUNT_32_BIT;
            break;
    }

    return VK_SAMPLE_COUNT_1_BIT;
}

inline VkBufferUsageFlags Conv(const BufferUsage& m)
{
    switch (m)
    {
        case BufferUsage::Vertex:
            return VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        case BufferUsage::Index:
            return VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        case BufferUsage::Texture:
            return VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        case BufferUsage::Staging:
            return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        case BufferUsage::Uniform:
            return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        case BufferUsage::SSBODevice:
            return VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        case BufferUsage::SSBOHost:
            return VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        case BufferUsage::IndirectDrawCmds:
            return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        case BufferUsage::IndirectDrawCount:
            return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                   VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        default:
            DEBUG_ASSERT(false);
    }
    return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
}

inline VkMemoryPropertyFlags Conv2MemFlags(const BufferUsage& m)
{
    switch (m)
    {
        case BufferUsage::Vertex:
        case BufferUsage::Index:
        case BufferUsage::Texture:
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        case BufferUsage::Staging:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        case BufferUsage::Uniform:
        case BufferUsage::SSBOHost:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        case BufferUsage::SSBODevice:
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        case BufferUsage::GenericDeviceVisible:
            return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        case BufferUsage::IndirectDrawCmds:
            return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        default:
            DEBUG_ASSERT(false);
    }
    return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
}
inline VmaMemoryUsage Conv2VmaMemFlags(const BufferUsage& m)
{
    switch (m)
    {
        case BufferUsage::Vertex:
            return VMA_MEMORY_USAGE_CPU_TO_GPU;
        case BufferUsage::Index:
            return VMA_MEMORY_USAGE_CPU_TO_GPU;
        case BufferUsage::Texture:
            return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        case BufferUsage::Staging:
            return VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        case BufferUsage::Uniform:
        case BufferUsage::SSBOHost:
            return VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        case BufferUsage::SSBODevice:
            return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        case BufferUsage::GenericDeviceVisible:
            return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        case BufferUsage::IndirectDrawCmds:
            return VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        default:
            DEBUG_ASSERT(false);
    }
    return VMA_MEMORY_USAGE_AUTO;
}

inline VkShaderStageFlagBits Conv(const ShaderTypeBits& m)
{
    u32 vkBits = 0;
    if ((u32)m & (u32)ShaderTypeBits::Vertex)
        vkBits = (vkBits | VK_SHADER_STAGE_VERTEX_BIT);
    if ((u32)m & (u32)ShaderTypeBits::Fragment)
        vkBits = (vkBits | VK_SHADER_STAGE_FRAGMENT_BIT);
    if ((u32)m & (u32)ShaderTypeBits::Compute)
        vkBits = (vkBits | VK_SHADER_STAGE_COMPUTE_BIT);
    if ((u32)m & (u32)ShaderTypeBits::Geometry)
        vkBits = (vkBits | VK_SHADER_STAGE_GEOMETRY_BIT);
    if ((u32)m & (u32)ShaderTypeBits::All)
        vkBits = VK_SHADER_STAGE_ALL;

    return (VkShaderStageFlagBits)vkBits;
}

static inline u32 Conv(SyncStages stage)
{
    u32 vkStage = 0;
    if (stage == SyncStages::NONE)
        return vkStage;
    if ((u32)stage & (u32)SyncStages::TOP_OF_PIPE)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
    if ((u32)stage & (u32)SyncStages::DRAW_INDIRECT)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT);
    if ((u32)stage & (u32)SyncStages::EARLY_FRAGMENT_TESTS)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT);
    if ((u32)stage & (u32)SyncStages::LATE_FRAGMENT_TESTS)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);
    if ((u32)stage & (u32)SyncStages::VERTEX_SHADER)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT);
    if ((u32)stage & (u32)SyncStages::FRAGMENT_SHADER)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    if ((u32)stage & (u32)SyncStages::COMPUTE_SHADER)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    if ((u32)stage & (u32)SyncStages::TRANSFER)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_TRANSFER_BIT);
    if ((u32)stage & (u32)SyncStages::COLOR_ATTACHMENT_OUTPUT)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    if ((u32)stage & (u32)SyncStages::BOTTOM_OF_PIPE)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
    if ((u32)stage & (u32)SyncStages::ALL_COMMANDS)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
    if ((u32)stage & (u32)SyncStages::DEPTH_OUTPUT)
        vkStage =
            (vkStage | VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);
            
    return vkStage;
}

static inline VkSamplerAddressMode Conv(TextureWrapMode mode)
{
    switch (mode)
    {
        case TextureWrapMode::REPEAT:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case TextureWrapMode::MIRRORED_REPEAT:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case TextureWrapMode::CLAMP_TO_EDGE:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case TextureWrapMode::CLAMP_TO_BORDER:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default:
            DEBUG_ASSERT(false);
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}