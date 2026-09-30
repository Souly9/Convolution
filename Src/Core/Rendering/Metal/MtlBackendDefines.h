#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/WindowManager.h"
#include "MtlForwardDecls.h"

static inline constexpr u64 MAX_TEXTURES = 4096;

#define MTL_DEVICE MtlGlobals::GetDevice()

// Releases a retained metal-cpp object and nulls the pointer
#define MTL_RELEASE_IF(res)                                                                                            \
    if (res != nullptr)                                                                                                \
    {                                                                                                                  \
        res->release();                                                                                                \
        res = nullptr;                                                                                                 \
    }

// Argument buffer tier 2 + MTLGPUFamilyApple7/Mac2 cover bindless and ray queries
struct MetalRequiredDeviceFeatures
{
    bool needsArgumentBuffersTier2{true};
    bool needsRayTracing{true};
};
static inline const MetalRequiredDeviceFeatures g_requiredMetalDeviceFeatures{};

// Maps to Vulkan validation layers; set MTL_DEBUG_LAYER=1 / MTL_SHADER_VALIDATION=1 in the environment
#ifdef NDEBUG
static inline constexpr bool ENABLE_METAL_VALIDATION = false;
#else
static inline constexpr bool ENABLE_METAL_VALIDATION = true;
#endif

#include "Core/Rendering/Core/RenderingTypeDefs.h"
