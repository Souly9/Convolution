#pragma once
#include <cassert>
#include <memory>
#include <EASTL/allocator.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/shared_ptr.h>

#define GLFW_INCLUDE_VULKAN

namespace eastl {}

namespace stltype = eastl;

const inline stltype::string ENGINE_NAME = "Convolution";
constexpr inline uint32_t CONVOLUTION_MAJOR = 1;

// memory
// Want to switch to custom allocators one day
struct VkAllocationCallbacks;
inline VkAllocationCallbacks* VulkanAllocator() { return nullptr; };

class VulkanBackend;
using RenderAPI = VulkanBackend;

#define DEBUG_LOG 
