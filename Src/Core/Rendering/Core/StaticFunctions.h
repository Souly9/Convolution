#pragma once
#include "Core/Global/GlobalDefines.h"

namespace SRF
{
	IMPLEMENT_GRAPHICS_API
	static inline void QueryImageForPresentationFromMainSwapchain(const Semaphore& imageAvailableSemaphore, const Fence& imageAvailableFence, u32& imageIndex, u64 timeout = UINT64_MAX);

	IMPLEMENT_GRAPHICS_API
	static inline void SubmitForPresentationToMainSwapchain(Semaphore* pWaitSemaphore, u32 swapChainIdx);
}

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkStaticFunctions.inl"
#endif