#pragma once
#include "BackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "VkGlobals.h"
#include "VkSynchronization.h"
#include <vulkan/vulkan_core.h>

namespace SRF
{
static void SubmitCommandBufferToQueue(const stltype::vector<CommandBuffer*>& commandBuffers,
                                       const Fence& transferFinishedFence,
                                       QueueType queue)
{
    stltype::vector<VkSubmitInfo2> submitInfos;
    submitInfos.reserve(commandBuffers.size());
    stltype::vector<VkSemaphoreSubmitInfo> waitSemaphoreInfos;
    waitSemaphoreInfos.reserve(50);
    stltype::vector<VkSemaphoreSubmitInfo> signalSemaphoreInfos;
    signalSemaphoreInfos.reserve(50);
    stltype::vector<VkCommandBufferSubmitInfo> commandBufferInfos;
    commandBufferInfos.reserve(commandBuffers.size());

    auto FillSemaphoreSubmitInfo = [](u32& infoCount,
                                      stltype::vector<VkSemaphoreSubmitInfo>& infoVector,
                                      const stltype::vector<RawSemaphoreHandle>& semaphores,
                                      u32 stageMask)
    {
        u32 addedSemaphores = 0;
        for (size_t i = 0; i < semaphores.size(); ++i)
        {
            if (semaphores[i] == VK_NULL_HANDLE)
                continue;
            VkSemaphoreSubmitInfo& waitInfo = infoVector.emplace_back();
            waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
            waitInfo.semaphore = semaphores[i];
            waitInfo.stageMask = stageMask;
            waitInfo.value = 0; // Binary semaphores use value 0
            waitInfo.pNext = nullptr;

            ++addedSemaphores;
        }
        infoCount = addedSemaphores;
    };

    for (const auto* pCmdBuffer : commandBuffers)
    {
        const auto& waitSemaphores = pCmdBuffer->GetWaitSemaphores();
        const auto& signalSemaphores = pCmdBuffer->GetSignalSemaphores();
        const auto waitStages = pCmdBuffer->GetWaitStages();
        const auto signalStages = pCmdBuffer->GetSignalStages();
        const auto& timelineWaits = pCmdBuffer->GetTimelineWaits();
        const auto& timelineSignals = pCmdBuffer->GetTimelineSignals();
        const bool hasBinary = !waitSemaphores.empty() || !signalSemaphores.empty();
        const bool hasTimeline = !timelineWaits.empty() || !timelineSignals.empty();

        // Track where our semaphore infos start in the vectors
        const size_t waitInfoStart = waitSemaphoreInfos.size();
        const size_t signalInfoStart = signalSemaphoreInfos.size();

        // Add timeline wait semaphores if present
        if (hasTimeline && !timelineWaits.empty())
        {
            for (const auto& wait : timelineWaits)
            {
                if (wait.semaphore == VK_NULL_HANDLE)
                    continue;
                VkSemaphoreSubmitInfo& waitInfo = waitSemaphoreInfos.emplace_back();
                waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                waitInfo.semaphore = wait.semaphore;
                waitInfo.stageMask = waitStages;
                waitInfo.value = wait.value;
                waitInfo.pNext = nullptr;
            }
        }

        // Add binary wait semaphores if present
        if (hasBinary && !waitSemaphores.empty())
        {
            u32 addedSemaphores = 0;
            FillSemaphoreSubmitInfo(addedSemaphores, waitSemaphoreInfos, waitSemaphores, waitStages);
        }

        // Add timeline signal semaphores if present
        if (hasTimeline && !timelineSignals.empty())
        {
            for (const auto& signal : timelineSignals)
            {
                if (signal.semaphore == VK_NULL_HANDLE)
                    continue;
                VkSemaphoreSubmitInfo& signalInfo = signalSemaphoreInfos.emplace_back();
                signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                signalInfo.semaphore = signal.semaphore;
                signalInfo.stageMask = signalStages;
                signalInfo.value = signal.value;
                signalInfo.pNext = nullptr;
            }
        }

        // Add binary signal semaphores if present
        if (hasBinary && !signalSemaphores.empty())
        {
            u32 addedSemaphores = 0;
            FillSemaphoreSubmitInfo(addedSemaphores, signalSemaphoreInfos, signalSemaphores, signalStages);
        }

        VkCommandBufferSubmitInfo& commandBufferInfo = commandBufferInfos.emplace_back();
        commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        commandBufferInfo.commandBuffer = pCmdBuffer->GetRef();

        VkSubmitInfo2& submitInfo = submitInfos.emplace_back();
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo.pNext = nullptr;

        submitInfo.commandBufferInfoCount = 1;
        submitInfo.pCommandBufferInfos = &commandBufferInfo;

        // Set wait semaphores (may include both timeline and binary)
        const size_t waitCount = waitSemaphoreInfos.size() - waitInfoStart;
        if (waitCount > 0)
        {
            submitInfo.waitSemaphoreInfoCount = static_cast<u32>(waitCount);
            submitInfo.pWaitSemaphoreInfos = &waitSemaphoreInfos[waitInfoStart];
        }
        else
        {
            submitInfo.waitSemaphoreInfoCount = 0;
            submitInfo.pWaitSemaphoreInfos = nullptr;
        }

        // Set signal semaphores (may include both timeline and binary)
        const size_t signalCount = signalSemaphoreInfos.size() - signalInfoStart;
        if (signalCount > 0)
        {
            submitInfo.signalSemaphoreInfoCount = static_cast<u32>(signalCount);
            submitInfo.pSignalSemaphoreInfos = &signalSemaphoreInfos[signalInfoStart];
        }
        else
        {
            submitInfo.signalSemaphoreInfoCount = 0;
            submitInfo.pSignalSemaphoreInfos = nullptr;
        }
    }

    VkQueue submitQueue;
    if (queue == QueueType::Transfer)
        submitQueue = VkGlobals::GetAllQueues().transfer;
    else if (queue == QueueType::Compute)
        submitQueue = VkGlobals::GetAllQueues().compute;
    else if (queue == QueueType::Graphics)
        submitQueue = VkGlobals::GetAllQueues().graphics;
    else
        DEBUG_ASSERT(false);

    DEBUG_ASSERT(vkQueueSubmit2(submitQueue, submitInfos.size(), submitInfos.data(), transferFinishedFence.GetRef()) ==
                 VK_SUCCESS);
}

template <>
static inline void QueryImageForPresentationFromMainSwapchain<Vulkan>(const Semaphore& imageAvailableSemaphore,
                                                                      const Fence& imageAvailableFence,
                                                                      u32& imageIndex,
                                                                      u64 timeout)
{
    VkResult rslt;
    do
    {
        // The timeout should typically be large (e.g., a billion nanoseconds or
        // UINT64_MAX for infinite)
        rslt = vkAcquireNextImageKHR(VkGlobals::GetLogicalDevice(),
                                     VkGlobals::GetMainSwapChain(),
                                     timeout,
                                     imageAvailableSemaphore.GetRef(),
                                     imageAvailableFence.GetRef(),
                                     &imageIndex);
    } while (rslt == VK_NOT_READY);
}

template <>
static void SubmitForPresentationToMainSwapchain<Vulkan>(Semaphore* pWaitSemaphore, u32 swapChainIdx)
{
    VkSemaphore waitSemaphore = VK_NULL_HANDLE;
    u32 waitCount = 0;

    if (pWaitSemaphore)
    {
        waitSemaphore = pWaitSemaphore->GetRef();
        waitCount = 1;
    }

    VkSwapchainKHR swapChains[] = {VkGlobals::GetMainSwapChain()};

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = waitCount;
    presentInfo.pWaitSemaphores = waitCount > 0 ? &waitSemaphore : nullptr;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &swapChainIdx;
    presentInfo.pResults = nullptr;

    DEBUG_ASSERT(vkQueuePresentKHR(VkGlobals::GetPresentQueue(), &presentInfo) == VK_SUCCESS);
}

template<>
static void WaitForDeviceIdle<Vulkan>()
{
    vkDeviceWaitIdle(VK_LOGICAL_DEVICE);
}

} // namespace SRF
