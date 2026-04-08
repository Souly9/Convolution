#pragma once
// Lightweight header with forward declarations and type aliases only
// Use this in headers that need rendering types but don't need complete definitions
// For complete types, include RenderingIncludes.h in your .cpp file

#include "Core/Global/GlobalDefines.h"

// Common struct definitions (API-agnostic)
struct MeshResourceData
{
    u32 vertBufferOffset;
    u32 indexBufferOffset;
    u32 vertCount;
    u32 indexCount;
};
using MeshHandle = MeshResourceData;

// Note: ImageLayout is defined in Core/Rendering/Core/Texture.h
// Include that file when you need ImageLayout

// API-specific forward declarations and type aliases
#ifdef USE_VULKAN
#include "vk_mem_alloc.h"
#include <vulkan/vulkan.h>

// Forward declarations - Vulkan implementations
class ColorAttachmentVulkan;
class DepthBufferAttachmentVulkan;
class GraphicsPipelineVulkan;
class ComputePipelineVulkan;
class ShaderVulkan;

class VkGPUTimingQuery;
class QueryPoolVulkan;

// Type aliases - abstract API-specific types
// ...
// Initial forward decl
template<typename API>
class GenBufferT;
template<typename API>
class StagingBufferT;
template<typename API>
class UniformBufferT;
template<typename API>
class StorageBufferT;
template<typename API>
class IndirectDrawCommandBufferT;
template<typename API>
class IndirectDrawCommandBufferCommon;
template<typename API>
class VertexBufferT;
template<typename API>
class IndexBufferT;
template<typename API>
class TextureT;
template<typename API>
class SemaphoreT;
template<typename API>
class FenceT;
template <typename API>
class TimelineSemaphoreT;
template <typename API>
class ComputePipelineT;
template <typename API>
class GraphicsPipelineT;
template <typename API>
class CommandPoolT;
template <typename API>
class CommandBufferT;
template <typename API>
class DescriptorPoolT;
template <typename API>
class DescriptorSetT;
template <typename API>
class DescriptorSetLayoutT;
template <typename API>
class AttachmentT;
template <typename API>
class ColorAttachmentT;
template <typename API>
class DepthAttachmentT;
template <typename API>
class GPUTimingQueryT;
template <typename API>
class ShaderT;

// Definitions of rendering types
using Texture = TextureT<CurrentAPI>;
using Semaphore = SemaphoreT<CurrentAPI>;
using Fence = FenceT<CurrentAPI>;
using TimelineSemaphore = TimelineSemaphoreT<CurrentAPI>;
using IndirectDrawCmdBuf = IndirectDrawCommandBufferCommon<CurrentAPI>;
using CommandPool = CommandPoolT<CurrentAPI>;
using CommandBuffer = CommandBufferT<CurrentAPI>;
using DescriptorPool = DescriptorPoolT<CurrentAPI>;
using DescriptorSet = DescriptorSetT<CurrentAPI>;
using DescriptorSetLayout = DescriptorSetLayoutT<CurrentAPI>;
using Attachment = AttachmentT<CurrentAPI>;
using ColorAttachment = ColorAttachmentT<CurrentAPI>;
using DepthAttachment = DepthAttachmentT<CurrentAPI>;
using GPUTimingQuery = GPUTimingQueryT<CurrentAPI>;
using Shader = ShaderT<CurrentAPI>;

using GenericBuffer = GenBufferT<CurrentAPI>;
using StagingBuffer = StagingBufferT<CurrentAPI>;
using UniformBuffer = UniformBufferT<CurrentAPI>;
using StorageBuffer = StorageBufferT<CurrentAPI>;
using IndirectDrawCommandBuffer = IndirectDrawCommandBufferT<CurrentAPI>;
using VertexBuffer = VertexBufferT<CurrentAPI>;
using IndexBuffer = IndexBufferT<CurrentAPI>;

using PSO = GraphicsPipelineT<CurrentAPI>;
using ComputePipeline = ComputePipelineT<CurrentAPI>;
using GPUMemoryHandle = VmaAllocation;
using GPUMappedMemoryHandle = void*;
using RawSemaphoreHandle = VkSemaphore;
using IndexedIndirectDrawCmd = VkDrawIndexedIndirectCommand;
using QueryPool = QueryPoolVulkan;
// Bindless texture handle type
using BindlessTextureHandle = u32;

// Texture format macro
#define TEXFORMAT(type) VK_FORMAT_##type

#endif // USE_VULKAN

// Note: Semaphore and Fence are defined via template pattern in Synchronization.h
// Include Synchronization.h directly when those types are needed
