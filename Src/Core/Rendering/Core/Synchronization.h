#pragma once
#include "RenderTraitsMacros.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Utils/EnumHelpers.h"
#include "Core/Rendering/Core/Resource.h"
#include "Core/Rendering/Vulkan/BackendDefines.h"

enum class SyncStages
{
    NONE = 0,
    TOP_OF_PIPE = 1 << 0,
    DRAW_INDIRECT = 1 << 1,
    EARLY_FRAGMENT_TESTS = 1 << 2,
    LATE_FRAGMENT_TESTS = 1 << 3,
    VERTEX_SHADER = 1 << 4,
    FRAGMENT_SHADER = 1 << 5,
    COMPUTE_SHADER = 1 << 6,
    TRANSFER = 1 << 7,
    COLOR_ATTACHMENT_OUTPUT = 1 << 8,
    BOTTOM_OF_PIPE = 1 << 9,
    DEPTH_OUTPUT = 1 << 10,
    CLEAR = 1 << 11,
    RAY_TRACING_SHADER = 1 << 12,
    ACCELERATION_STRUCTURE_BUILD = 1 << 13,
    ALL_COMMANDS = 1 << 14,
};
MAKE_FLAG_ENUM(SyncStages)

class GPUSyncer : public TrackedResource
{
};

class CPUSyncer : public TrackedResource
{
};

class SemaphoreBase : public GPUSyncer
{
};

class FenceBase : public CPUSyncer
{
};

class TimelineSemaphoreBase : public GPUSyncer
{
};

#include "APITraits.h"
#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkSynchronization.h"
#include "Core/Rendering/Vulkan/VulkanTraits.h"
#endif

template <typename API>
class FenceT : public APITraits<API>::FenceType
{
public:
    // Inherit constructors
    using APITraits<API>::FenceType::FenceType;
    DECLARE_RENDER_RESOURCE_TRAITS(FenceT, FenceType)
};
template <typename API>
class SemaphoreT : public APITraits<API>::SemaphoreType
{
public:
    // Inherit constructors
    using APITraits<API>::SemaphoreType::SemaphoreType;
    DECLARE_RENDER_RESOURCE_TRAITS(SemaphoreT, SemaphoreType)
};
template <typename API>
class TimelineSemaphoreT : public APITraits<API>::TimelineSemaphoreType
{
public:
    // Inherit constructors
    using APITraits<API>::TimelineSemaphoreType::TimelineSemaphoreType;
    DECLARE_RENDER_RESOURCE_TRAITS(TimelineSemaphoreT, TimelineSemaphoreType)
};
