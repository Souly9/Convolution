#pragma once
#include "eathread/eathread.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "Typedefs.h"

// Configuration Macros
#ifndef CONVOLUTION_DUMP_RENDERGRAPH
#define CONVOLUTION_DUMP_RENDERGRAPH 1
#endif

#ifdef CONV_DEBUG
#define ASSERT(x)                                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            printf("ASSERT FAILED: %s at %s:%d\n", #x, __FILE__, __LINE__);                                            \
            abort();                                                                                                   \
        }                                                                                                              \
    } while (0)
#else
#define ASSERT(x)                                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            printf("ASSERT FAILED: %s at %s:%d\n", #x, __FILE__, __LINE__);                                            \
            abort();                                                                                                   \
        }                                                                                                              \
    } while (0)
#endif
#define DEBUG_ASSERT(x) ASSERT(x)

class AvailableRenderBackends
{
};
class Vulkan : public AvailableRenderBackends
{
};
class Metal : public AvailableRenderBackends
{
};

// Constants

const static inline stltype::string ENGINE_NAME = "Convolution";
constexpr static inline u32 FRAMES_IN_FLIGHT = 2u;
constexpr static inline u32 SWAPCHAIN_IMAGES = 2u;
constexpr static inline u32 CSM_INITIAL_CASCADES = 3u;
constexpr static inline mathstl::Vector2 CSM_DEFAULT_RES = mathstl::Vector2(2048.0f, 2048.0f);
constexpr static inline u32 MAX_BINDLESS_TEXTURES = 16536;
// Bindless texture slots from here up hold persistent (render target) textures
constexpr static inline u32 PERSISTENT_BINDLESS_REGION_START = 14000;
constexpr static inline u32 MAX_MESHES = 8192;

#define DEPTH_BUFFER_FORMAT TexFormat::D32_SFLOAT

// Compiled-in graphics API; query it at runtime through Renderer::GetAPI()/IsVulkan()/IsMetal()
#include "Core/Rendering/Core/APITraits.h"
#ifdef USE_VULKAN
using RenderAPI = Vulkan;
using CurrentAPI = API_Vulkan;
#elif defined(USE_METAL)
using RenderAPI = Metal;
using CurrentAPI = API_Metal;
#else
#error "No render backend selected (USE_VULKAN / USE_METAL)"
#endif

#define IMPLEMENT_GRAPHICS_API                                                                                         \
    template <class BackendAPI>                                                                                        \
        requires stltype::is_base_of_v<AvailableRenderBackends, BackendAPI>

// Logic Macros
#define COMP_ID(component) ECS::ComponentID<ECS::Components::component>::ID

static inline constexpr f32 FLOAT_TOLERANCE = 0.00001f;
static inline constexpr f32 AMBIENT_STRENGTH = 0.03f;

static inline u32 CORE_COUNT_AVAILABLE = 8;
