#pragma once
#include "Core/Global/GlobalDefines.h"

enum class Allocator
{
    // Just allocating each block through vulkans allocator functions, so will run against the limits super fast
    Default,
    // Classic vulkan memory allocator library
    VMA,
    // Own memory allocator, which I will implement one day TM
    Convolution
};

IMPLEMENT_GRAPHICS_API
class GPUMemManager
{
};

using GPUMemoryManager = GPUMemManager<RenderAPI>;

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkGPUMemoryManager.h"
#endif
