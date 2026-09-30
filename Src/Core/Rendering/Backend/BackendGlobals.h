#pragma once
// Active backend's globals and profiler, so shared code doesn't include backend headers directly
#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "Core/Rendering/Vulkan/VkProfiler.h"
using RenderGlobals = VkGlobals;
using BackendProfiler = VkProfiler;
#elif defined(USE_METAL)
#include "Core/Rendering/Metal/MtlGlobals.h"
#include "Core/Rendering/Metal/MtlProfiler.h"
using RenderGlobals = MtlGlobals;
using BackendProfiler = MtlProfiler;
#endif
