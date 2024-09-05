#pragma once
#include "Core/Global/GlobalDefines.h"

IMPLEMENT_GRAPHICS_API
class ShaderImpl
{
};

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkShader.h"
#endif

using Shader = ShaderImpl<RenderAPI>;