#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Utils/MemoryUtilities.h"

// Generic attachment for render passes
IMPLEMENT_GRAPHICS_API
class RPass
{
};


using RenderPass = RPass<RenderAPI>;

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkRenderPass.h"
#endif