#pragma once
#include "Core/Global/GlobalDefines.h"

class GPUSyncer : public TrackedResource
{

};

IMPLEMENT_GRAPHICS_API
class SemaphoreImpl : public GPUSyncer 
{
};


class CPUSyncer : public TrackedResource
{
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