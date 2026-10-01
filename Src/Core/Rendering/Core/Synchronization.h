#pragma once
#include "RenderTraitsMacros.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Utils/EnumHelpers.h"
#include "Core/Rendering/Core/Resource.h"

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
    HOST = 1 << 15,
};
MAKE_FLAG_ENUM(SyncStages)

enum class AccessFlags : u64
{
    NONE = 0,
    INDIRECT_COMMAND_READ = 1ULL << 0,
    INDEX_READ = 1ULL << 1,
    VERTEX_ATTRIBUTE_READ = 1ULL << 2,
    UNIFORM_READ = 1ULL << 3,
    INPUT_ATTACHMENT_READ = 1ULL << 4,
    SHADER_READ = 1ULL << 5,
    SHADER_WRITE = 1ULL << 6,
    COLOR_ATTACHMENT_READ = 1ULL << 7,
    COLOR_ATTACHMENT_WRITE = 1ULL << 8,
    DEPTH_STENCIL_ATTACHMENT_READ = 1ULL << 9,
    DEPTH_STENCIL_ATTACHMENT_WRITE = 1ULL << 10,
    TRANSFER_READ = 1ULL << 11,
    TRANSFER_WRITE = 1ULL << 12,
    HOST_READ = 1ULL << 13,
    HOST_WRITE = 1ULL << 14,
    MEMORY_READ = 1ULL << 15,
    MEMORY_WRITE = 1ULL << 16,
    SHADER_STORAGE_READ = 1ULL << 17,
    SHADER_STORAGE_WRITE = 1ULL << 18,
    ACCELERATION_STRUCTURE_READ = 1ULL << 19,
    ACCELERATION_STRUCTURE_WRITE = 1ULL << 20,
};
MAKE_FLAG_ENUM(AccessFlags)

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
#elif defined(USE_METAL)
#include "Core/Rendering/Metal/MetalTraits.h"
#include "Core/Rendering/Metal/MtlSynchronization.h"
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
