#pragma once
#include "Core/Global/GlobalDefines.h"
#include "VertexDefines.h"

#ifdef USE_VULKAN
class GenBufferVulkan;
class VertexBufferVulkan;
class CBufferVulkan;
class CommandPoolVulkan;
class AttachmentBaseVulkan;
class ColorAttachmentVulkan;
class FrameBufferVulkan;
class TextureVulkan;
class RenderPassVulkan;
class PipelineVulkan;
class ShaderVulkan;

using GenericBuffer = GenBufferVulkan;
using VertexBuffer = VertexBufferVulkan;
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

#include "Core/Rendering/Vulkan/VkShader.h"
#include "Core/Rendering/Vulkan/VkBuffer.h"
#include "Core/Rendering/Vulkan/VkTexture.h"
#include "Core/Rendering/Vulkan/VkAttachment.h"
#include "Core/Rendering/Vulkan/VkFramebuffer.h"
#include "Core/Rendering/Vulkan/VkRenderPass.h"
#include "Core/Rendering/Vulkan/VkCommandBuffer.h"
#include "Core/Rendering/Vulkan/VkCommandPool.h"
#include "Core/Rendering/Vulkan/VkPipeline.h"
#endif