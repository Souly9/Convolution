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
class IndirectDrawCommandBuffer; // Direct class, not an alias
class StorageBuffer;             // Direct class
class StagingBuffer;             // Direct class
class UniformBuffer;             // Direct class

// Type aliases - abstract API-specific types
using GenericBuffer = GenBufferVulkan;
using VertexBuffer = VertexBufferVulkan;
using IndexBuffer = IndexBufferVulkan;
using CommandBuffer = CBufferVulkan;
using CommandPool = CommandPoolVulkan;
using Texture = TextureVulkan;
using PSO = GraphicsPipelineVulkan;
using ComputePipeline = ComputePipelineVulkan;
using Shader = ShaderVulkan;
using GPUMemoryHandle = VmaAllocation;
using GPUMappedMemoryHandle = void*;
using DescriptorPool = DescriptorPoolVulkan;
using DescriptorSet = DescriptorSetVulkan;
using DescriptorSetLayout = DescriptorSetLayoutVulkan;
using ColorAttachment = ColorAttachmentVulkan;
using DepthAttachment = DepthBufferAttachmentVulkan;
using RawSemaphoreHandle = VkSemaphore;
using IndexedIndirectDrawCmd = VkDrawIndexedIndirectCommand;

// Bindless texture handle type
using BindlessTextureHandle = u32;

// Texture format macro
#define TEXFORMAT(type) VK_FORMAT_##type

#endif // USE_VULKAN

// Note: Semaphore and Fence are defined via template pattern in Synchronization.h
// Include Synchronization.h directly when those types are needed
