#pragma once
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <EASTL/allocator.h>
#include <EAStdC/EAString.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/variant.h>
#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <EASTL/bit.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/optional.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <SimpleMath/SimpleMath.h>
#include <cassert>
#include <functional>
#include <EASTL/hash_map.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/fixed_string.h>
#include <EAThread/eathread_thread.h>
#include <EAThread/eathread_futex.h>
#include <EASTL/bitset.h>
#include <EASTL/fixed_function.h>
#include "Core/../../Shaders/Globals/BindingSlots.h"

#if CONV_DEBUG
#define ASSERT(x) assert(x)
#else 
#undef CONV_DEBUG
#define ASSERT(x) x
#endif
#define DEBUG_ASSERT(x) ASSERT(x)
#define PROFILING_ENABLED 1

namespace stltype = eastl;
namespace threadSTL = EA::Thread;
namespace mathstl = DirectX::SimpleMath;

#include "Typedefs.h"

const static inline stltype::string ENGINE_NAME = "Convolution";
constexpr static inline u32 FRAMES_IN_FLIGHT = 2u;
constexpr static inline u32 SWAPCHAIN_IMAGES = 2u;
constexpr static inline u32 MAX_BINDLESS_TEXTURES = 16536;
constexpr static inline u32 MAX_MESHES = 4096;
constexpr static inline u32 MAX_TILES = 1;
constexpr static inline u32 MAX_LIGHTS_PER_TILE = 32;

// memory
// Want to switch to custom allocators one day
struct VkAllocationCallbacks;
inline VkAllocationCallbacks* VulkanAllocator() { return nullptr; };

class AvailableRenderBackends {};
class Vulkan : AvailableRenderBackends {};
using RenderAPI = Vulkan;

#define USE_VULKAN

#ifdef USE_VULKAN

#define SEPERATE_TRANSFERQUEUE true
#define GLFW_INCLUDE_VULKAN
#define CONV_MIN_VULKAN_VERSION VK_API_VERSION_1_4
#define CONV_DESIRED_VULKAN_VERSION VK_API_VERSION_1_4

#endif

#define IMPLEMENT_GRAPHICS_API template<class BackendAPI> requires stltype::is_base_of_v<AvailableRenderBackends, BackendAPI>
#define GLOBAL_INCLUDES 

#include "Core/ECS/Entity.h"
#include "Core/Global/State/States.h"

#include "LogDefines.h"
#include "GlobalVariables.h"
#include "State/ApplicationState.h"
#include "Core/Events/EventSystem.h"

#define CUR_FRAME FrameGlobals::GetFrameNumber()
#define COMP_ID(component) ECS::ComponentID<ECS::Components::##component>::ID

static inline constexpr f32 FLOAT_TOLERANCE = 0.00001f;

static inline constexpr f32 AMBIENT_STRENGTH = 0.1f;

#define MAKE_FLAG_ENUM(name) static inline name operator|(name lhs, name rhs) { return static_cast<name>(static_cast<char>(lhs) | static_cast<char>(rhs)); }


#include "Profiling.h"