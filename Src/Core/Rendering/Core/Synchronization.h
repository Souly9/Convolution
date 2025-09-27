#pragma once
#include "Core/Global/GlobalDefines.h"

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
};
class GPUSyncer : public TrackedResource
{

};

IMPLEMENT_GRAPHICS_API
class SemaphoreImpl : public GPUSyncer 
{
};


class CPUSyncer : public TrackedResource
{
protected:
#ifdef CONV_DEBUG
	bool m_signaled{ false };
#endif
};

IMPLEMENT_GRAPHICS_API
class FenceImpl : public CPUSyncer
{

};


using Semaphore = SemaphoreImpl<Vulkan>;
using Fence = FenceImpl<Vulkan>;

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkSynchronization.h"
#endif