#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Synchronization.h"

namespace SRF
{
enum class SwapchainPresentStatus
{
    Presented,
    NeedsRecreate,
    Failed,
};

enum class SwapchainAcquireStatus
{
    Acquired,
    NeedsRecreate,
    Failed,
};

IMPLEMENT_GRAPHICS_API
static inline SwapchainAcquireStatus QueryImageForPresentationFromMainSwapchain(
    const Semaphore& imageAvailableSemaphore,
    const Fence& imageAvailableFence,
    u32& imageIndex,
    u64 timeout = UINT64_MAX);

IMPLEMENT_GRAPHICS_API
static inline SwapchainPresentStatus SubmitForPresentationToMainSwapchain(Semaphore* pWaitSemaphore, u32 swapChainIdx);

IMPLEMENT_GRAPHICS_API
static inline void WaitForDeviceIdle();
} // namespace SRF

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkStaticFunctions.inl"
#endif
