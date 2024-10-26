#pragma once
#include "Core/Global/GlobalDefines.h"

namespace SRF
{
	IMPLEMENT_GRAPHICS_API
	static inline void QueryImageForPresentationFromMainSwapchain(const Semaphore& imageAvailableSemaphore, u32& imageIndex, u64 timeout = UINT64_MAX);

	IMPLEMENT_GRAPHICS_API
	static inline void SubmitForPresentationToMainSwapchain(const Semaphore& waitSemaphore, const u32 swapChainIdx);

	IMPLEMENT_GRAPHICS_API
	static inline void SubmitCommandBufferToGraphicsQueue(const Semaphore& waitSemaphore, const Semaphore& signalSemaphore, const CommandBuffer* cmdBuffer, const Fence& inFlightFence);

	IMPLEMENT_GRAPHICS_API
	static inline void SubmitCommandBufferToTransferQueue(const CommandBuffer* cmdBuffer, const Fence& transferFinishedFence);

	IMPLEMENT_GRAPHICS_API
	static inline void SubmitCommandBufferToGraphicsQueue(const CommandBuffer* cmdBuffer, const Fence& transferFinishedFence);
}

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkStaticFunctions.inl"
#endif