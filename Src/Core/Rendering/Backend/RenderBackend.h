#pragma once
#include "Core/Global/GlobalDefines.h"

#include "RenderBackendBase.h"

using RenderBackend = RenderBackendImpl<RenderAPI>;

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VulkanBackend.h"
#endif