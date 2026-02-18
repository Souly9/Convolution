#pragma once
#include "Core/Rendering/Core/APITraits.h"

// Forward Declarations
class GenBufferVulkan;
class VertexBufferVulkan;
class IndexBufferVulkan;
class CBufferVulkan;
class CommandPoolVulkan;
class ColorAttachmentVulkan;
class DepthBufferAttachmentVulkan;
class TextureVulkan;
class GraphicsPipelineVulkan;
class ComputePipelineVulkan;
class ShaderVulkan;
class DescriptorSetVulkan;
class DescriptorPoolVulkan;
class DescriptorSetLayoutVulkan;
class IndirectDrawCommandBufferVulkan;
class StorageBuffer;
class StagingBuffer;
class UniformBuffer;
class VkGPUTimingQuery;
class QueryPoolVulkan;

template<>
struct APITraits<API_Vulkan>
{
    using TextureType = TextureVulkan;
    using BufferType = GenBufferVulkan;
    using CommandBufferType = CBufferVulkan;
    using GraphicsPipelineType = GraphicsPipelineVulkan;
    using ComputePipelineType = ComputePipelineVulkan;
    using IndirectDrawCommandBufferType = IndirectDrawCommandBufferVulkan;
    // Add other types as we migrate them...
};
