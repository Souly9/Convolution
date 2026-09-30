#pragma once
#include "Core/Global/GlobalDefines.h"

#ifdef USE_VULKAN
class VkTextureManager;
using TextureManager = VkTextureManager;
#include "Core/Rendering/Vulkan/VkTextureManager.h"
#elif defined(USE_METAL)
class MtlTextureManager;
using TextureManager = MtlTextureManager;
#include "Core/Rendering/Metal/MtlTextureManager.h"
#endif
