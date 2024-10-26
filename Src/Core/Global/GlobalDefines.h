#pragma once
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
#include <cassert>
#include <functional>
#include <EASTL/hash_map.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/fixed_string.h>

#ifndef NDEBUG
#define ASSERT(x) assert(x)
#else 
#undef CONV_DEBUG
#define ASSERT(x) x
#endif
#define DEBUG_ASSERT(x) ASSERT(x)

namespace eastl {}

namespace stltype = eastl;

#include "Typedefs.h"

const static inline stltype::string ENGINE_NAME = "Convolution";
constexpr static inline u32 FRAMES_IN_FLIGHT = 2u;
constexpr static inline u32 MAX_BINDLESS_TEXTURES = 16536;
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
#define CONV_MIN_VULKAN_VERSION VK_API_VERSION_1_1
#define CONV_DESIRED_VULKAN_VERSION VK_API_VERSION_1_3

#endif

#define IMPLEMENT_GRAPHICS_API template<class BackendAPI> requires stltype::is_base_of_v<AvailableRenderBackends, BackendAPI>
#define GLOBAL_INCLUDES 

#include "LogDefines.h"
#include "GlobalVariables.h"