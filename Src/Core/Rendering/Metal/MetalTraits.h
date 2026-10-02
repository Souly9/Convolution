#pragma once
#include "Core/Rendering/Core/APITraits.h"

// Forward Declarations
class GenBufferMetal;
class CBufferMetal;
class CommandPoolMetal;
class TextureMetal;
class GraphicsPipelineMetal;
class ComputePipelineMetal;
class DescriptorSetMetal;
class DescriptorPoolMetal;
class DescriptorSetLayoutMetal;
class QueryPoolMetal;
class SemaphoreMetal;
class FenceMetal;
class TimelineSemaphoreMetal;
class StagingBufferMetal;
class UniformBufferMetal;
class StorageBufferMetal;
class IndirectDrawCommandBufferMetal;
class VertexBufferMetal;
class IndexBufferMetal;
class GPUTimingQueryMetal;
class ShaderMetal;
class AccelerationStructureMetal;
class MtlTracyGPUManager;

template<>
struct APITraits<API_Metal>
{
    using TextureType = TextureMetal;
    using BufferType = GenBufferMetal;
    using CommandBufferType = CBufferMetal;
    using GraphicsPipelineType = GraphicsPipelineMetal;
    using ComputePipelineType = ComputePipelineMetal;
    using SemaphoreType = SemaphoreMetal;
    using FenceType = FenceMetal;
    using TimelineSemaphoreType = TimelineSemaphoreMetal;
    using CommandPoolType = CommandPoolMetal;
    using StagingBufferType = StagingBufferMetal;
    using UniformBufferType = UniformBufferMetal;
    using StorageBufferType = StorageBufferMetal;
    using IndirectDrawCommandBufferType = IndirectDrawCommandBufferMetal;
    using VertexBufferType = VertexBufferMetal;
    using IndexBufferType = IndexBufferMetal;
    using DescriptorSetType = DescriptorSetMetal;
    using DescriptorPoolType = DescriptorPoolMetal;
    using DescriptorSetLayoutType = DescriptorSetLayoutMetal;
    using GPUTimingQueryType = GPUTimingQueryMetal;
    using ShaderType = ShaderMetal;
    using AccelerationStructureType = AccelerationStructureMetal;
    using TracyGPUManagerType = MtlTracyGPUManager;
};
