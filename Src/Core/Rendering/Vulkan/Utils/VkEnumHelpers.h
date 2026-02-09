#pragma once
#include "Core/Rendering/Core/Buffer.h"
#include "Core/Rendering/Core/Defines/UBODefines.h"
#include "Core/Rendering/Core/Pipeline.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/Texture.h"
#include <vk_mem_alloc.h>

#include "TextureEnums.h"

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

inline VkCullModeFlagBits Conv(const Cullmode& m)
{
    switch (m)
    {
        case Cullmode::None:
            return VK_CULL_MODE_NONE;
            break;
        case Cullmode::Front:
            return VK_CULL_MODE_FRONT_BIT;
            break;
        case Cullmode::Back:
            return VK_CULL_MODE_BACK_BIT;
            break;
        case Cullmode::Both:
            return VK_CULL_MODE_FRONT_AND_BACK;
            break;
    }

    return VK_CULL_MODE_FRONT_BIT;
}

inline VkFrontFace Conv(const FrontFace& m)
{
    switch (m)
    {
        case FrontFace::CounterClockwise:
            return VK_FRONT_FACE_COUNTER_CLOCKWISE;
            break;
        case FrontFace::Clockwise:
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
        case LoadOp::IDC:
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
        case StoreOp::IDC:
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
        case ImageLayout::SHADER_READ_OPTIMAL:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            break;
        case ImageLayout::PRESENT:
            return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            break;
        case ImageLayout::COLOR_ATTACHMENT:
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            break;
        case ImageLayout::DEPTH_STENCIL:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
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

inline VkImageUsageFlags Conv(const Usage& m)
{
    switch (m)
    {
        case Usage::GBuffer:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        case Usage::ColorAttachment:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        case Usage::DepthAttachment:
            return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        case Usage::TransferSrc:
            return VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        case Usage::TransferDst:
            return VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        case Usage::Sampled:
            return VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        case Usage::AttachmentReadWrite:
            return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        case Usage::StencilAttachment:
            return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        case Usage::ShadowMap:
            return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                   VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            break;
        default:
            DEBUG_ASSERT(false);
            return 0;
    }
}

inline VkImageTiling Conv(const Tiling& m)
{
    switch (m)
    {
        case Tiling::OPTIMAL:
            return VK_IMAGE_TILING_OPTIMAL;
            break;
        case Tiling::LINEAR:
            return VK_IMAGE_TILING_LINEAR;
            break;
    }
    DEBUG_ASSERT(false);
    return VK_IMAGE_TILING_OPTIMAL;
}

static inline u32 Conv(SyncStages stage)
{
    u32 vkStage = 0;
    if (stage == SyncStages::NONE)
        return vkStage;
    if ((u32)stage & (u32)SyncStages::TOP_OF_PIPE)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT);
    else if ((u32)stage & (u32)SyncStages::DRAW_INDIRECT)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT);
    else if ((u32)stage & (u32)SyncStages::EARLY_FRAGMENT_TESTS)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT);
    else if ((u32)stage & (u32)SyncStages::LATE_FRAGMENT_TESTS)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);
    else if ((u32)stage & (u32)SyncStages::VERTEX_SHADER)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT);
    else if ((u32)stage & (u32)SyncStages::FRAGMENT_SHADER)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT);
    else if ((u32)stage & (u32)SyncStages::COMPUTE_SHADER)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
    else if ((u32)stage & (u32)SyncStages::COLOR_ATTACHMENT_OUTPUT)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT);
    else if ((u32)stage & (u32)SyncStages::BOTTOM_OF_PIPE)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
    else if ((u32)stage & (u32)SyncStages::ALL_COMMANDS)
        vkStage = (vkStage | VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT);
    else if ((u32)stage & (u32)SyncStages::DEPTH_OUTPUT)
        vkStage =
            (vkStage | VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT);
    else
        DEBUG_ASSERT(false);
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