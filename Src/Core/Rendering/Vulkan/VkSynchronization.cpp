#include "VkGlobals.h"
#include "VkSynchronization.h"

SemaphoreImpl<Vulkan>::~SemaphoreImpl()
{
    TRACKED_DESC_IMPL
}

void SemaphoreImpl<Vulkan>::Create()
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    DEBUG_ASSERT(vkCreateSemaphore(VkGlobals::GetLogicalDevice(), &semaphoreInfo, VulkanAllocator(), &m_semaphore) == VK_SUCCESS);
}

void SemaphoreImpl<Vulkan>::CleanUp()
{
    VK_FREE_IF(m_semaphore, vkDestroySemaphore(VkGlobals::GetLogicalDevice(), m_semaphore, VulkanAllocator()))
}

VkSemaphore SemaphoreImpl<Vulkan>::GetRef() const
{
    return m_semaphore;
}

FenceImpl<Vulkan>::~FenceImpl()
{
    TRACKED_DESC_IMPL
}

void FenceImpl<Vulkan>::Create(bool signaled)
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
    DEBUG_ASSERT(vkCreateFence(VkGlobals::GetLogicalDevice(), &fenceInfo, VulkanAllocator(), &m_fence) == VK_SUCCESS);
}

void FenceImpl<Vulkan>::CleanUp()
{
    VK_FREE_IF(m_fence, vkDestroyFence(VkGlobals::GetLogicalDevice(), m_fence, VulkanAllocator()))
}

void FenceImpl<Vulkan>::WaitFor(const u64& timeout) const
{
    vkWaitForFences(VkGlobals::GetLogicalDevice(), 1, &m_fence, VK_TRUE, timeout);
}

void FenceImpl<Vulkan>::Reset()
{
    vkResetFences(VkGlobals::GetLogicalDevice(), 1, &m_fence);
}

VkFence FenceImpl<Vulkan>::GetRef() const
{
    return m_fence;
}
