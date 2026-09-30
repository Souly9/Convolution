#pragma once
#include "Core/Global/GlobalDefines.h"

#include "RenderBackendBase.h"

using RenderBackend = RenderBackendImpl<RenderAPI>;

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VulkanBackend.h"
#elif defined(USE_METAL)
#include "Core/Rendering/Metal/MetalBackend.h"
#endif