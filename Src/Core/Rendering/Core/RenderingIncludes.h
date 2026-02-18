#pragma once
// Complete type includes - use in .cpp files or where complete type definitions are required
// For headers, prefer RenderingForwardDecls.h to avoid circular dependencies

#include "RenderingForwardDecls.h"

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VulkanTraits.h"
#include "Core/Rendering/Vulkan/VkAttachment.h"
#include "Core/Rendering/Vulkan/VkBuffer.h"
#include "Core/Rendering/Vulkan/VkCommandBuffer.h"
#include "Core/Rendering/Vulkan/VkCommandPool.h"
#include "Core/Rendering/Vulkan/VkDescriptorPool.h"
#include "Core/Rendering/Vulkan/VkDescriptorSetLayout.h"
#include "Core/Rendering/Vulkan/VkPipeline.h"
#include "Core/Rendering/Vulkan/VkShader.h"
#include "Core/Rendering/Vulkan/VkSynchronization.h"
#include "Core/Rendering/Vulkan/VkTexture.h"
#include "RenderingData.h"
#endif
