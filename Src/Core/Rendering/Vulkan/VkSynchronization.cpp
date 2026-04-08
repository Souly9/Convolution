#include "VkSynchronization.h"
#include "VkGlobals.h"

SemaphoreVulkan::~SemaphoreVulkan()
{
    TRACKED_DESC_IMPL
}

void SemaphoreVulkan::Create()
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    DEBUG_ASSERT(vkCreateSemaphore(VK_LOGICAL_DEVICE, &semaphoreInfo, VulkanAllocator(), &m_semaphore) == VK_SUCCESS);
}

void SemaphoreVulkan::CleanUp(){
    VK_FREE_IF(m_semaphore, vkDestroySemaphore(VK_LOGICAL_DEVICE, m_semaphore, VulkanAllocator()))}

VkSemaphore SemaphoreVulkan::GetRef() const
{
    return m_semaphore;
}

void SemaphoreVulkan::NamingCallBack(const stltype::string& name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_SEMAPHORE;
    nameInfo.objectHandle = (uint64_t)GetRef();
    nameInfo.pObjectName = name.c_str();

    vkSetDebugUtilsObjectName(VK_LOGICAL_DEVICE, &nameInfo);
}

FenceVulkan::~FenceVulkan()
{
    TRACKED_DESC_IMPL
}

void FenceVulkan::Create(bool signaled)
{
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;
    DEBUG_ASSERT(vkCreateFence(VK_LOGICAL_DEVICE, &fenceInfo, VulkanAllocator(), &m_fence) == VK_SUCCESS);
#ifdef CONV_DEBUG
    m_signaled = signaled;
#endif
}

void FenceVulkan::CleanUp()
{
    VK_FREE_IF(m_fence, vkDestroyFence(VK_LOGICAL_DEVICE, m_fence, VulkanAllocator()))
}

void FenceVulkan::WaitFor(const u64& timeout) const
{
    const VkResult result = vkWaitForFences(VK_LOGICAL_DEVICE, 1, &m_fence, VK_TRUE, timeout);
    DEBUG_ASSERT(result == VK_SUCCESS);
}

bool FenceVulkan::IsSignaled() const
{
    auto rslt = vkGetFenceStatus(VK_LOGICAL_DEVICE, m_fence);
    if (rslt == VK_SUCCESS)
    {
        return true;
    }
    return false;
}

void FenceVulkan::Reset()
{
    vkResetFences(VK_LOGICAL_DEVICE, 1, &m_fence);
#ifdef CONV_DEBUG
    m_signaled = false;
#endif
}

VkFence FenceVulkan::GetRef() const
{
    return m_fence;
}

void FenceVulkan::NamingCallBack(const stltype::string& name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_FENCE;
    nameInfo.objectHandle = (uint64_t)GetRef();
    nameInfo.pObjectName = name.c_str();

    vkSetDebugUtilsObjectName(VK_LOGICAL_DEVICE, &nameInfo);
}

// Timeline Semaphore Implementation
TimelineSemaphoreVulkan::~TimelineSemaphoreVulkan()
{
    TRACKED_DESC_IMPL
}

void TimelineSemaphoreVulkan::Create(u64 initialValue)
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

void TimelineSemaphoreVulkan::CleanUp(){
    VK_FREE_IF(m_semaphore, vkDestroySemaphore(VK_LOGICAL_DEVICE, m_semaphore, VulkanAllocator()))}

VkSemaphore TimelineSemaphoreVulkan::GetRef() const
{
    return m_semaphore;
}

u64 TimelineSemaphoreVulkan::GetValue() const
{
    u64 value = 0;
    const VkResult result = vkGetSemaphoreCounterValue(VK_LOGICAL_DEVICE, m_semaphore, &value);
    DEBUG_ASSERT(result == VK_SUCCESS);
    return value;
}

void TimelineSemaphoreVulkan::HostSignal(u64 value)
{
    VkSemaphoreSignalInfo signalInfo{};
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    signalInfo.semaphore = m_semaphore;
    signalInfo.value = value;

    DEBUG_ASSERT(vkSignalSemaphore(VK_LOGICAL_DEVICE, &signalInfo) == VK_SUCCESS);
}

void TimelineSemaphoreVulkan::Wait(u64 value, u64 timeout) const
{
    VkSemaphoreWaitInfo waitInfo{};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &m_semaphore;
    waitInfo.pValues = &value;

    const VkResult result = vkWaitSemaphores(VK_LOGICAL_DEVICE, &waitInfo, timeout);
    if (timeout > 0)
    {
        DEBUG_ASSERT(result == VK_SUCCESS || result == VK_ERROR_DEVICE_LOST);
    }
    else
    {
        DEBUG_ASSERT(result == VK_SUCCESS || result == VK_TIMEOUT || result == VK_ERROR_DEVICE_LOST);
    }
}

void TimelineSemaphoreVulkan::NamingCallBack(const stltype::string& name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_SEMAPHORE;
    nameInfo.objectHandle = (uint64_t)GetRef();
    nameInfo.pObjectName = name.c_str();

    vkSetDebugUtilsObjectName(VK_LOGICAL_DEVICE, &nameInfo);
}
