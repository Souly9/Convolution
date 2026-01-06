#pragma once
#include "BackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "VkGlobals.h"
#include "VkSynchronization.h"
#include <vulkan/vulkan_core.h>


namespace SRF {
static void SubmitCommandBufferToQueue(
    const stltype::vector<CommandBuffer *> &commandBuffers,
    const Fence &transferFinishedFence, QueueType queue) {

  stltype::vector<VkSubmitInfo2> submitInfos;
  submitInfos.reserve(commandBuffers.size());
  stltype::vector<VkSemaphoreSubmitInfo> waitSemaphoreInfos;
  waitSemaphoreInfos.reserve(50);
  stltype::vector<VkSemaphoreSubmitInfo> signalSemaphoreInfos;
  signalSemaphoreInfos.reserve(50);
  stltype::vector<VkCommandBufferSubmitInfo> commandBufferInfos;
  commandBufferInfos.reserve(commandBuffers.size());

  auto FillSemaphoreSubmitInfo =
      [](u32 &infoCount, stltype::vector<VkSemaphoreSubmitInfo> &infoVector,
         const stltype::vector<RawSemaphoreHandle> &semaphores, u32 stageMask) {
        u32 addedSemaphores = 0;
        for (size_t i = 0; i < semaphores.size(); ++i) {
          if (semaphores[i] == VK_NULL_HANDLE)
            continue;
          VkSemaphoreSubmitInfo &waitInfo = infoVector.emplace_back();
          waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
          waitInfo.semaphore = semaphores[i];
          waitInfo.stageMask = stageMask;
          waitInfo.value = 0; // For binary semaphores, value is 0
          waitInfo.pNext = nullptr;

          ++addedSemaphores;
        }
        infoCount = addedSemaphores;
      };

  for (const auto *pCmdBuffer : commandBuffers) {
    const auto &waitSemaphores = pCmdBuffer->GetWaitSemaphores();
    const auto &signalSemaphores = pCmdBuffer->GetSignalSemaphores();
    const auto waitStages = pCmdBuffer->GetWaitStages();
    const auto signalStages = pCmdBuffer->GetSignalStages();

    VkCommandBufferSubmitInfo &commandBufferInfo =
        commandBufferInfos.emplace_back();
    commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commandBufferInfo.commandBuffer = pCmdBuffer->GetRef();

    VkSubmitInfo2 &submitInfo = submitInfos.emplace_back();
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.pNext = nullptr;

    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos =
        &commandBufferInfo; // Pointers to the structs you've filled

    if (waitSemaphores.empty() == false) {
      u32 addedSemaphores = 0;
      FillSemaphoreSubmitInfo(addedSemaphores, waitSemaphoreInfos,
                              waitSemaphores, waitStages);
      if (addedSemaphores > 0) {
        submitInfo.waitSemaphoreInfoCount = addedSemaphores;
        submitInfo.pWaitSemaphoreInfos =
            &waitSemaphoreInfos.at(waitSemaphoreInfos.size() - addedSemaphores);
      }
    } else {
      submitInfo.waitSemaphoreInfoCount = 0;
      submitInfo.pWaitSemaphoreInfos = nullptr;
    }

    if (signalSemaphores.empty() == false) {
      u32 addedSemaphores = 0;
      FillSemaphoreSubmitInfo(addedSemaphores, signalSemaphoreInfos,
                              signalSemaphores, signalStages);
      if (addedSemaphores > 0) {
        submitInfo.signalSemaphoreInfoCount = addedSemaphores;
        submitInfo.pSignalSemaphoreInfos = &signalSemaphoreInfos.at(
            signalSemaphoreInfos.size() - addedSemaphores);
      }
    } else {
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

  // submitQueue = VkGlobals::GetAllQueues().graphics;
  DEBUG_ASSERT(vkQueueSubmit2(submitQueue, submitInfos.size(),
                              submitInfos.data(),
                              transferFinishedFence.GetRef()) == VK_SUCCESS);
}

template <>
static inline void QueryImageForPresentationFromMainSwapchain<Vulkan>(
    const Semaphore &imageAvailableSemaphore, const Fence &imageAvailableFence,
    u32 &imageIndex, u64 timeout) {
  VkResult rslt;
  do {
    // The timeout should typically be large (e.g., a billion nanoseconds or
    // UINT64_MAX for infinite)
    rslt = vkAcquireNextImageKHR(VkGlobals::GetLogicalDevice(),
                                 VkGlobals::GetMainSwapChain(), timeout,
                                 imageAvailableSemaphore.GetRef(),
                                 imageAvailableFence.GetRef(), &imageIndex);
  } while (rslt == VK_NOT_READY);
}

template <>
static void
SubmitForPresentationToMainSwapchain<Vulkan>(Semaphore *pWaitSemaphore,
                                             u32 swapChainIdx) {
  VkSemaphore waitSemaphores = pWaitSemaphore->GetRef();
  VkSwapchainKHR swapChains[] = {VkGlobals::GetMainSwapChain()};

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = &waitSemaphores;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;
  presentInfo.pImageIndices = &swapChainIdx;
  presentInfo.pResults = nullptr;

  DEBUG_ASSERT(vkQueuePresentKHR(VkGlobals::GetPresentQueue(), &presentInfo) ==
               VK_SUCCESS);
}
} // namespace SRF
