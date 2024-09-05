#pragma once
#include <EASTL/allocator.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <EASTL/bit.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/optional.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <cassert>

#define GLFW_INCLUDE_VULKAN

namespace eastl {}

namespace stltype = eastl;

const static inline stltype::string ENGINE_NAME = "Convolution";
constexpr static inline uint32_t CONVOLUTION_MAJOR = 1;

// memory
// Want to switch to custom allocators one day
struct VkAllocationCallbacks;
inline VkAllocationCallbacks* VulkanAllocator() { return nullptr; };

class AvailableRenderBackends {};
class Vulkan : AvailableRenderBackends {};
using RenderAPI = Vulkan;

#define USE_VULKAN

#define IMPLEMENT_GRAPHICS_API template<class BackendAPI> requires stltype::is_base_of_v<AvailableRenderBackends, BackendAPI>
#define GLOBAL_INCLUDES 

#include "Typedefs.h"
#include "LogDefines.h"
#include "GlobalVariables.h"

#define DEBUG_ASSERT(x) assert(x)

#ifdef CONV_DEBUG
#define ASSERT(x) assert(x)
#else 
#define ASSERT(x) x
#endif