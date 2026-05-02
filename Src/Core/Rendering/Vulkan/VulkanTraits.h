#pragma once
#include "Core/Rendering/Core/APITraits.h"

// Forward Declarations
class GenBufferVulkan;
class CBufferVulkan;
class CommandPoolVulkan;
class TextureVulkan;
class GraphicsPipelineVulkan;
class ComputePipelineVulkan;
class DescriptorSetVulkan;
class DescriptorPoolVulkan;
class DescriptorSetLayoutVulkan;
class QueryPoolVulkan;
class SemaphoreVulkan;
class FenceVulkan;
class TimelineSemaphoreVulkan;
class StagingBufferVulkan;
class UniformBufferVulkan;
class StorageBufferVulkan;
class IndirectDrawCommandBufferVulkan;
class VertexBufferVulkan;
class IndexBufferVulkan;
class AttachmentBaseVulkan;
class ColorAttachmentVulkan;
class DepthAttachmentVulkan;
class GPUTimingQueryVulkan;
class ShaderVulkan;
class AccelerationStructureVulkan;

template<>
struct APITraits<API_Vulkan>
{
    using TextureType = TextureVulkan;
    using BufferType = GenBufferVulkan;
    using CommandBufferType = CBufferVulkan;
    using GraphicsPipelineType = GraphicsPipelineVulkan;
    using ComputePipelineType = ComputePipelineVulkan;
    using IndirectDrawCommandBufferType = IndirectDrawCommandBufferVulkan;
    using SemaphoreType = SemaphoreVulkan;
    using FenceType = FenceVulkan;
    using TimelineSemaphoreType = TimelineSemaphoreVulkan;
    using CommandPoolType = CommandPoolVulkan;
    using StagingBufferType = StagingBufferVulkan;
    using UniformBufferType = UniformBufferVulkan;
    using StorageBufferType = StorageBufferVulkan;
    using IndirectDrawCommandBufferType = IndirectDrawCommandBufferVulkan;
    using VertexBufferType = VertexBufferVulkan;
    using IndexBufferType = IndexBufferVulkan;
    using DescriptorSetType = DescriptorSetVulkan;
    using DescriptorPoolType = DescriptorPoolVulkan;
    using DescriptorSetLayoutType = DescriptorSetLayoutVulkan;
    using AttachmentType = AttachmentBaseVulkan;
    using ColorAttachmentType = ColorAttachmentVulkan;
    using DepthAttachmentType = DepthAttachmentVulkan;
    using GPUTimingQueryType = GPUTimingQueryVulkan;
    using ShaderType = ShaderVulkan;
    using AccelerationStructureType = AccelerationStructureVulkan;
    // Add other types as we migrate them...
};
