#pragma once
#include "Core/Global/GlobalDefines.h"

// Backend type aliases for shared headers; the only API-select site needed outside the core trait headers
#ifdef USE_VULKAN
class VkTextureManager;
using TextureManager = VkTextureManager;
class ConvAllocatorSimple;
using GPUMemoryManager = ConvAllocatorSimple;
#elif defined(USE_METAL)
class MtlTextureManager;
using TextureManager = MtlTextureManager;
class MtlGPUMemoryManager;
using GPUMemoryManager = MtlGPUMemoryManager;
#else
#error "No render backend selected (USE_VULKAN / USE_METAL)"
#endif

template <typename API>
class RenderBackendImpl;
using RenderBackend = RenderBackendImpl<RenderAPI>;
