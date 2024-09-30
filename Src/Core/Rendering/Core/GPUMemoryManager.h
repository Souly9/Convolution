#pragma once
#include "Core/Global/GlobalDefines.h"

IMPLEMENT_GRAPHICS_API
class GPUMemManager
{

};

using GPUMemoryManager = GPUMemManager<RenderAPI>;

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkGPUMemoryManager.h"
#endif
