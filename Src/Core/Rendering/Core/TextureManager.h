#pragma once
#include "Core/Global/GlobalDefines.h"

#include "Core/Rendering/Backend/BackendForwardDecls.h"

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkTextureManager.h"
#elif defined(USE_METAL)
#include "Core/Rendering/Metal/MtlTextureManager.h"
#endif
