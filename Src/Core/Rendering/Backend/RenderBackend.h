#pragma once
#include "Core/Global/GlobalDefines.h"

template<typename API>
class RenderBackendImpl
{
};


using RenderBackend = RenderBackendImpl<RenderAPI>;

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VulkanBackend.h"
#endif