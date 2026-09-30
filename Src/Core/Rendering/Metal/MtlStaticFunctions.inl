#pragma once
#include "MtlBackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/StaticFunctions.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/RenderDefinitions.h"

// Swapchain acquire/present map to CAMetalLayer::nextDrawable and MTL::CommandBuffer::presentDrawable
namespace SRF
{
// TODO(Metal): commit MTL::CommandBuffers with encodeWait/encodeSignalEvent; fence via SharedEvent
inline void SubmitCommandBufferToQueue(const stltype::vector<CommandBuffer*>& commandBuffers,
                                       const Fence& transferFinishedFence,
                                       QueueType queue)
{
}

template <>
inline SwapchainAcquireStatus QueryImageForPresentationFromMainSwapchain<Metal>(const Semaphore& imageAvailableSemaphore,
                                                                              const Fence& imageAvailableFence,
                                                                              u32& imageIndex,
                                                                              u64 timeout)
{
    // TODO(Metal)
    return SwapchainAcquireStatus::Failed;
}

template <>
inline SwapchainPresentStatus SubmitForPresentationToMainSwapchain<Metal>(Semaphore* pWaitSemaphore, u32 swapChainIdx)
{
    // TODO(Metal)
    return SwapchainPresentStatus::Failed;
}

template <>
inline void WaitForDeviceIdle<Metal>()
{
    // TODO(Metal): no device-wide idle; wait on the last committed command buffer per queue
}
} // namespace SRF
