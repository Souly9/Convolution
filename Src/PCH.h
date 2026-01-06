#pragma once

// Standard Library
#include <cassert>

// EASTL
#include <EASTL/allocator.h>
#include <EASTL/array.h>
#include <EASTL/bit.h>
#include <EASTL/bitset.h>
#include <EASTL/fixed_function.h>
#include <EASTL/fixed_string.h>
#include <EASTL/fixed_vector.h>
#include <EASTL/functional.h>
#include <EASTL/hash_map.h>
#include <EASTL/optional.h>
#include <EASTL/queue.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/variant.h>
#include <EASTL/vector.h>

// DirectX Math
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <SimpleMath/SimpleMath.h>

// EA StdC
#include <EAStdC/EAString.h>

// EA Thread
#include <EAThread/eathread_futex.h>
#include <EAThread/eathread_thread.h>

// Vulkan / GLFW
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

// Internal Base Types - Unified common header
#include "Core/Global/CoreCommon.h"
#include "Core/Global/EASTLFormatters.h"
