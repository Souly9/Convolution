#pragma once
#include "Core/Global/GlobalDefines.h"

#include "RenderBackendBase.h"
#include "BackendForwardDecls.h"

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VulkanBackend.h"
#elif defined(USE_METAL)
#include "Core/Rendering/Metal/MetalBackend.h"
#endif