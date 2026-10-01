#pragma once
#include <cstdint>
#include <vulkan/vulkan_core.h>

struct QueueFamilyIndices;
class VkProfiler;
class VkTracyGPUManager;

struct VulkanQueues
{
    VkQueue graphics;
    VkQueue present;
    VkQueue transfer;
    VkQueue compute;
};

// Device state of the Vulkan backend owned by g_renderer; Vulkan-only code reads it through these.
// Light header (only vulkan_core.h) so inline Vulkan helpers can use it without include cycles.
namespace VkBackend
{
VkInstance Instance();
VkPhysicalDevice PhysicalDevice();
VkDevice Device();
VkQueue GraphicsQueue();
VkQueue PresentQueue();
VulkanQueues Queues();
VkSwapchainKHR Swapchain();
QueueFamilyIndices QueueFamilies();
// Graphics records the uploads, graphics and compute read buffers and images; count is 1 when they share a family
const uint32_t* SharedQueueFamilies(uint32_t& count);
const VkPhysicalDeviceProperties& DeviceProperties();
const VkPhysicalDeviceMemoryProperties& MemoryProperties();
VkProfiler* Profiler();
VkTracyGPUManager* TracyManager();

template <typename CreateInfo>
inline void SetSharedQueueFamilies(CreateInfo& info)
{
    uint32_t count = 0;
    const uint32_t* pFamilies = SharedQueueFamilies(count);
    info.sharingMode = count > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = count > 1 ? count : 0;
    info.pQueueFamilyIndices = count > 1 ? pFamilies : nullptr;
}
} // namespace VkBackend
