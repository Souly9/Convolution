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

// Declared only; backends specialize it
IMPLEMENT_GRAPHICS_API
class GPUMemManager;

#include "Core/Rendering/Backend/BackendForwardDecls.h"

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkGPUMemoryManager.h"
#elif defined(USE_METAL)
#include "Core/Rendering/Metal/MtlGPUMemoryManager.h"
#endif
