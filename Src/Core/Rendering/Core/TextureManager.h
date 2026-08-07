#pragma once
#include "Core/Global/GlobalDefines.h"

#ifdef USE_VULKAN
class VkTextureManager;
using TextureManager = VkTextureManager;
#include "Core/Rendering/Vulkan/VkTextureManager.h"
#endif
