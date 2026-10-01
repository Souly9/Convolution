#pragma once
#include "Core/Global/GlobalDefines.h"

// Backend type aliases for shared headers; the only API-select site needed outside the core trait headers
#ifdef USE_VULKAN
class VkTextureManager;
using TextureManager = VkTextureManager;
#elif defined(USE_METAL)
class MtlTextureManager;
using TextureManager = MtlTextureManager;
#else
#error "No render backend selected (USE_VULKAN / USE_METAL)"
#endif

template <typename API>
class RenderBackendImpl;
using RenderBackend = RenderBackendImpl<RenderAPI>;

IMPLEMENT_GRAPHICS_API
class GPUMemManager;
using GPUMemoryManager = GPUMemManager<RenderAPI>;
