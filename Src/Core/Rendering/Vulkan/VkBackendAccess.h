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
const VkPhysicalDeviceProperties& DeviceProperties();
const VkPhysicalDeviceMemoryProperties& MemoryProperties();
VkProfiler* Profiler();
VkTracyGPUManager* TracyManager();
} // namespace VkBackend
