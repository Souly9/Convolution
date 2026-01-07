#include "VkSynchronization.h"
#include "VkGlobals.h"

SemaphoreImpl<Vulkan>::~SemaphoreImpl()
{
    TRACKED_DESC_IMPL
}

void SemaphoreImpl<Vulkan>::Create()
{
    VkSemaphoreTypeCreateInfo timelineInfo{};
    timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_BINARY;
    timelineInfo.initialValue = 0;
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = &timelineInfo;
    DEBUG_ASSERT(vkCreateSemaphore(VK_LOGICAL_DEVICE, &semaphoreInfo, VulkanAllocator(), &m_semaphore) == VK_SUCCESS);
}

void SemaphoreImpl<Vulkan>::CleanUp(){
    VK_FREE_IF(m_semaphore, vkDestroySemaphore(VK_LOGICAL_DEVICE, m_semaphore, VulkanAllocator()))}

VkSemaphore SemaphoreImpl<Vulkan>::GetRef() const
{
    return m_semaphore;
}

void SemaphoreImpl<Vulkan>::NamingCallBack(const stltype::string& name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_SEMAPHORE;
    nameInfo.objectHandle = (uint64_t)GetRef();
    nameInfo.pObjectName = name.c_str();

    vkSetDebugUtilsObjectName(VK_LOGICAL_DEVICE, &nameInfo);
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
    DEBUG_ASSERT(vkCreateFence(VK_LOGICAL_DEVICE, &fenceInfo, VulkanAllocator(), &m_fence) == VK_SUCCESS);
#ifdef CONV_DEBUG
    m_signaled = signaled;
#endif
}

void FenceImpl<Vulkan>::CleanUp()
{
    VK_FREE_IF(m_fence, vkDestroyFence(VK_LOGICAL_DEVICE, m_fence, VulkanAllocator()))
}

void FenceImpl<Vulkan>::WaitFor(const u64& timeout) const
{
    vkWaitForFences(VK_LOGICAL_DEVICE, 1, &m_fence, VK_TRUE, timeout);
}

bool FenceImpl<Vulkan>::IsSignaled() const
{
    auto rslt = vkGetFenceStatus(VK_LOGICAL_DEVICE, m_fence);
    if (rslt == VK_SUCCESS)
    {
        return true;
    }
    return false;
}

void FenceImpl<Vulkan>::Reset()
{
    vkResetFences(VK_LOGICAL_DEVICE, 1, &m_fence);
#ifdef CONV_DEBUG
    m_signaled = false;
#endif
}

VkFence FenceImpl<Vulkan>::GetRef() const
{
    return m_fence;
}

void FenceImpl<Vulkan>::NamingCallBack(const stltype::string& name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_FENCE;
    nameInfo.objectHandle = (uint64_t)GetRef();
    nameInfo.pObjectName = name.c_str();

    vkSetDebugUtilsObjectName(VK_LOGICAL_DEVICE, &nameInfo);
}

// Timeline Semaphore Implementation
TimelineSemaphoreImpl<Vulkan>::~TimelineSemaphoreImpl()
{
    TRACKED_DESC_IMPL
}

void TimelineSemaphoreImpl<Vulkan>::Create(u64 initialValue)
{
    VkSemaphoreTypeCreateInfo timelineInfo{};
    timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineInfo.initialValue = initialValue;

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.pNext = &timelineInfo;

    DEBUG_ASSERT(vkCreateSemaphore(VK_LOGICAL_DEVICE, &semaphoreInfo, VulkanAllocator(), &m_semaphore) == VK_SUCCESS);
}

void TimelineSemaphoreImpl<Vulkan>::CleanUp(){
    VK_FREE_IF(m_semaphore, vkDestroySemaphore(VK_LOGICAL_DEVICE, m_semaphore, VulkanAllocator()))}

VkSemaphore TimelineSemaphoreImpl<Vulkan>::GetRef() const
{
    return m_semaphore;
}

u64 TimelineSemaphoreImpl<Vulkan>::GetValue() const
{
    u64 value = 0;
    vkGetSemaphoreCounterValue(VK_LOGICAL_DEVICE, m_semaphore, &value);
    return value;
}

void TimelineSemaphoreImpl<Vulkan>::HostSignal(u64 value)
{
    VkSemaphoreSignalInfo signalInfo{};
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    signalInfo.semaphore = m_semaphore;
    signalInfo.value = value;

    DEBUG_ASSERT(vkSignalSemaphore(VK_LOGICAL_DEVICE, &signalInfo) == VK_SUCCESS);
}

void TimelineSemaphoreImpl<Vulkan>::Wait(u64 value, u64 timeout) const
{
    VkSemaphoreWaitInfo waitInfo{};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &m_semaphore;
    waitInfo.pValues = &value;

    vkWaitSemaphores(VK_LOGICAL_DEVICE, &waitInfo, timeout);
}

void TimelineSemaphoreImpl<Vulkan>::NamingCallBack(const stltype::string& name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_SEMAPHORE;
    nameInfo.objectHandle = (uint64_t)GetRef();
    nameInfo.pObjectName = name.c_str();

    vkSetDebugUtilsObjectName(VK_LOGICAL_DEVICE, &nameInfo);
}
