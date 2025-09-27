#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Defines/VertexDefines.h"
#include "View.h"

#ifdef USE_VULKAN
#define TEXFORMAT(type) VK_FORMAT_##type

#include "vk_mem_alloc.h"
class GenBufferVulkan;
class VertexBufferVulkan;
class IndexBufferVulkan;
class CBufferVulkan;
class CommandPoolVulkan;
class AttachmentBaseVulkan;
class ColorAttachmentVulkan;
class TextureVulkan;
class PipelineVulkan;
class ShaderVulkan;
class DescriptorSetVulkan;
class DescriptorPoolVulkan;
class DescriptorSetLayoutVulkan;
struct VkDrawIndexedIndirectCommand;
class DepthBufferAttachmentVulkan;

using GenericBuffer = GenBufferVulkan;
using VertexBuffer = VertexBufferVulkan;
using IndexBuffer = IndexBufferVulkan;
using CommandBuffer = CBufferVulkan;
using CommandPool = CommandPoolVulkan;
using Texture = TextureVulkan;
using PSO = PipelineVulkan;
using Shader = ShaderVulkan;

using GPUMemoryHandle = VmaAllocation;
using GPUMappedMemoryHandle = void*;
using DescriptorPool = DescriptorPoolVulkan;
using TextureHandle = u32;
// Mainly used to index into the global bindless texture array
using BindlessTextureHandle = u32;
using DescriptorSetLayout = DescriptorSetLayoutVulkan;
using ColorAttachment = ColorAttachmentVulkan;
using DepthAttachment = DepthBufferAttachmentVulkan;

using RawSemaphoreHandle = VkSemaphore;
// Indirect drawing stuff
using IndexedIndirectDrawCmd = VkDrawIndexedIndirectCommand;

#include "Core/Rendering/Vulkan/VkShader.h"
#include "Core/Rendering/Vulkan/VkBuffer.h"
#include "Core/Rendering/Vulkan/VkTexture.h"
#include "Core/Rendering/Vulkan/VkAttachment.h"
#include "Core/Rendering/Vulkan/VkCommandBuffer.h"
#include "Core/Rendering/Vulkan/VkCommandPool.h"
#include "Core/Rendering/Vulkan/VkDescriptorPool.h"
#include "Core/Rendering/Vulkan/VkPipeline.h"
#include "Core/Rendering/Vulkan/VkDescriptorSetLayout.h"
#endif

#include "RenderingData.h"