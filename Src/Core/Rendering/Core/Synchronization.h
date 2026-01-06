#pragma once
#include "Core/Global/GlobalDefines.h"
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
    COLOR_ATTACHMENT_OUTPUT = 1 << 8,
    BOTTOM_OF_PIPE = 1 << 9,
    ALL_COMMANDS = 1 << 10,
    DEPTH_OUTPUT = 1 << 11,
};

class GPUSyncer : public TrackedResource
{
};

class CPUSyncer : public TrackedResource
{
protected:
    void* m_fenceEvent{nullptr};
};

// Define primary templates declaration (defined by macro)
IMPLEMENT_GRAPHICS_API
class SemaphoreImpl : public GPUSyncer
{
};

IMPLEMENT_GRAPHICS_API
class FenceImpl : public CPUSyncer
{
};

// Declare specializations for Vulkan
template <>
class SemaphoreImpl<Vulkan>;

template <>
class FenceImpl<Vulkan>;

using Semaphore = SemaphoreImpl<Vulkan>;
using Fence = FenceImpl<Vulkan>;
using CPUSynchObject = FenceImpl<Vulkan>;

// Note: VkSynchronization.h provides the DEFINITIONS of the specializations.
// It is NOT included here to avoid circular dependencies.
// Include "RenderingIncludes.h" or "VkSynchronization.h" in .cpp files where complete types are needed.