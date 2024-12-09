#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Defines/VertexDefines.h"
#include "View.h"

#ifdef USE_VULKAN
class GenBufferVulkan;
class VertexBufferVulkan;
class IndexBufferVulkan;
class CBufferVulkan;
class CommandPoolVulkan;
class AttachmentBaseVulkan;
class ColorAttachmentVulkan;
class FrameBufferVulkan;
class TextureVulkan;
class RenderPassVulkan;
class PipelineVulkan;
class ShaderVulkan;
class DescriptorSetVulkan;
class DescriptorPoolVulkan;
class DescriptorSetLayoutVulkan;

using GenericBuffer = GenBufferVulkan;
using VertexBuffer = VertexBufferVulkan;
using IndexBuffer = IndexBufferVulkan;
using CommandBuffer = CBufferVulkan;
using CommandPool = CommandPoolVulkan;
using RenderPassAttachment = AttachmentBaseVulkan;
using RenderPassAttachmentColor = ColorAttachmentVulkan;
using FrameBuffer = FrameBufferVulkan;
using Texture = TextureVulkan;
using RenderPass = RenderPassVulkan;
using PSO = PipelineVulkan;
using Shader = ShaderVulkan;
using GPUMemoryHandle = VkDeviceMemory;
using GPUMappedMemoryHandle = void*;
using DescriptorPool = DescriptorPoolVulkan;
using TextureHandle = u64;
using DescriptorSetLayout = DescriptorSetLayoutVulkan;

#include "Core/Rendering/Vulkan/VkShader.h"
#include "Core/Rendering/Vulkan/VkBuffer.h"
#include "Core/Rendering/Vulkan/VkTexture.h"
#include "Core/Rendering/Vulkan/VkAttachment.h"
#include "Core/Rendering/Vulkan/VkCommandBuffer.h"
#include "Core/Rendering/Vulkan/VkCommandPool.h"
#include "Core/Rendering/Vulkan/VkDescriptorPool.h"
#include "Core/Rendering/Vulkan/VkPipeline.h"
#include "Core/Rendering/Vulkan/VkFramebuffer.h"
#include "Core/Rendering/Vulkan/VkRenderPass.h"
#include "Core/Rendering/Vulkan/VkDescriptorSetLayout.h"
#endif