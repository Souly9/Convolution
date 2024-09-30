#pragma once
#include <vulkan/vulkan_core.h>
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "BackendDefines.h"
#include "VkSynchronization.h"
#include "VkGlobals.h"

namespace SRF
{
	template<>
	static inline void QueryImageForPresentationFromMainSwapchain<Vulkan>(const Semaphore& imageAvailableSemaphore, u32& imageIndex, u64 timeout)
	{
		vkAcquireNextImageKHR(VkGlobals::GetLogicalDevice(), VkGlobals::GetMainSwapChain(), timeout, imageAvailableSemaphore.GetRef(), VK_NULL_HANDLE, &imageIndex);
	}

	template<>
	static inline void SubmitForPresentationToMainSwapchain<Vulkan>(const Semaphore& waitSemaphore, const u32 swapChainImageIdx)
	{
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		VkSemaphore waitSemaphores[] = { waitSemaphore.GetRef()};
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = waitSemaphores;
		VkSwapchainKHR swapChains[] = { VkGlobals::GetMainSwapChain() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &swapChainImageIdx;
		presentInfo.pResults = nullptr; // Optional
		vkQueuePresentKHR(VkGlobals::GetPresentQueue(), &presentInfo);
	}

	template<>
	static inline void SubmitCommandBufferToGraphicsQueue<Vulkan>(const Semaphore& waitSemaphore, const Semaphore& signalSemaphore, const CBufferVulkan* cmdBuffer, const Fence& inFlightFence)
	{
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { waitSemaphore.GetRef() };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages; 
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuffer->GetRef();
		VkSemaphore signalSemaphores[] = { signalSemaphore.GetRef() };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		DEBUG_ASSERT(vkQueueSubmit(VkGlobals::GetGraphicsQueue(), 1, &submitInfo, inFlightFence.GetRef()) == VK_SUCCESS);
	}

	template<>
	static inline void SubmitCommandBufferToTransferQueue<Vulkan>(const CommandBuffer* cmdBuffer, const Fence& transferFinishedFence)
	{
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &cmdBuffer->GetRef();

		vkQueueSubmit(VkGlobals::GetAllQueues().transfer, 1, &submitInfo, transferFinishedFence.GetRef());
	}
}
