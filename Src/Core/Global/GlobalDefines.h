#pragma once

#ifdef NDEBUG
#undef NDEBUG
#endif

// Note: External libraries and common system headers are now in PCH.h
#include "Typedefs.h"
#include <cassert>

// Configuration Macros
#ifdef CONV_DEBUG
#define ASSERT(x) assert(x)
#else
#undef CONV_DEBUG
#define ASSERT(x) x
#endif
#define DEBUG_ASSERT(x) ASSERT(x)

// Forward Decls
struct VkAllocationCallbacks;
inline VkAllocationCallbacks* VulkanAllocator()
{
    return nullptr;
};

class AvailableRenderBackends
{
};
class Vulkan : AvailableRenderBackends
{
};
using RenderAPI = Vulkan;

// Constants
const static inline stltype::string ENGINE_NAME = "Convolution";
constexpr static inline u32 FRAMES_IN_FLIGHT = 2u;
constexpr static inline u32 SWAPCHAIN_IMAGES = 2u;
constexpr static inline u32 CSM_INITIAL_CASCADES = 7u;
constexpr static inline mathstl::Vector2 CSM_DEFAULT_RES = mathstl::Vector2(4096.0f, 4096.0f);
constexpr static inline u32 MAX_BINDLESS_TEXTURES = 16536;
constexpr static inline u32 MAX_MESHES = 4096;
constexpr static inline u32 MAX_MATERIALS = 256;
constexpr static inline u32 MAX_ENTITIES = 65536 * 10;
// Clustered lighting
constexpr static inline u32 MAX_SCENE_LIGHTS = 1024;
constexpr static inline u32 MAX_CLUSTERS = 16 * 32 * 64;
constexpr static inline u32 MAX_LIGHT_INDICES = MAX_CLUSTERS * 128;

// Definitions
#define USE_VULKAN

#ifdef USE_VULKAN
#define SEPERATE_TRANSFERQUEUE true
#define GLFW_INCLUDE_VULKAN
#define CONV_MIN_VULKAN_VERSION     VK_API_VERSION_1_4
#define CONV_DESIRED_VULKAN_VERSION VK_API_VERSION_1_4
#endif

#define IMPLEMENT_GRAPHICS_API                                                                                         \
    template <class BackendAPI>                                                                                        \
        requires stltype::is_base_of_v<AvailableRenderBackends, BackendAPI>

// Logic Macros
#define CUR_FRAME          FrameGlobals::GetFrameNumber()
#define COMP_ID(component) ECS::ComponentID<ECS::Components::##component>::ID

static inline constexpr f32 FLOAT_TOLERANCE = 0.00001f;
static inline constexpr f32 AMBIENT_STRENGTH = 0.1f;
