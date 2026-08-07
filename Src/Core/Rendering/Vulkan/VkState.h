#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/ConvolutionState.h"
#include "VkGPUMemoryManager.h"
#include "VkTextureManager.h"

class VkProfiler;
class VkTracyGPUManager;

struct Queues
{
    VkQueue graphics;
    VkQueue present;
    VkQueue transfer;
    VkQueue compute;
};

struct VulkanContext
{
    VkInstance Instance;
    VkPhysicalDevice PhysicalDevice;
    VkDevice Device;
    VkSampleCountFlagBits MSAASamples;
};

class VkState
{
public:
    static stltype::unique_ptr<GPUMemManager<Vulkan>> pGPUMemoryManager;
    static stltype::unique_ptr<VkTracyGPUManager> pTracyGPUManager;

    static VkDevice GetLogicalDevice();
    static VkProfiler* GetProfiler();
    static VkTracyGPUManager* GetTracyManager();
    static VkSwapchainKHR GetMainSwapChain();
    static VkQueue GetPresentQueue();
    static VkQueue GetGraphicsQueue();
    static Queues GetAllQueues();
    static const stltype::vector<Texture*>& GetSwapChainImages();
    static QueueFamilyIndices GetQueueFamilyIndices();
    static VkPhysicalDevice GetPhysicalDevice();
    static const VkPhysicalDeviceProperties& GetPhysicalDeviceProperties();
    static const VkPhysicalDeviceMemoryProperties& GetPhysicalDeviceMemoryProperties();
    static u64 GetTotalVram();
    static Texture* GetDepthStencilBuffer();
    static void SetContext(const VulkanContext& context);
    static void SetProfiler(VkProfiler* pProfiler);
    static void SetTracyManager(VkTracyGPUManager* pTracyMgr);

    static void SetPhysicalDeviceProperties(const VkPhysicalDeviceProperties& physDeviceProps);
    static void SetPhysicalDeviceMemoryProperties(const VkPhysicalDeviceMemoryProperties& memProps);
    static void SetLogicalDevice(VkDevice physDevice);
    static void SetMainSwapChain(const VkSwapchainKHR swapChain);
    static void SetPresentQueue(const VkQueue presentQueue);
    static void SetGraphicsQueue(const VkQueue graphicsQueue);
    static void SetAllQueues(const Queues& queues);
    static void SetSwapChainImages(const stltype::vector<Texture*>& images);
    static void SetQueueFamilyIndices(const QueueFamilyIndices& indices);
    static void SetPhysicalDevice(const VkPhysicalDevice& physDevice);
    static void SetDepthStencilBuffer(Texture* pDepthTex);
    static const VulkanContext& GetContext();

private:
    static stltype::vector<Texture*> s_swapChainImages;
    static VkProfiler* s_pProfiler;
    static VkTracyGPUManager* s_pTracyManager;
    static Texture* s_pDepthStencilBuffer;
    static QueueFamilyIndices s_indices;
    static VkDevice s_logicalDevice;
    static VkSwapchainKHR s_mainSwapChain;
    static VkQueue s_presentQueue;
    static VkQueue s_graphicsQueue;
    static Queues s_queues;
    static VkPhysicalDevice s_physicalDevice;
    static VkPhysicalDeviceProperties s_physicalDeviceProperties;
    static VkPhysicalDeviceMemoryProperties s_physicalDeviceMemoryProperties;
    static u64 s_totalVram;
    static VulkanContext s_context;
};

#define g_pGPUMemoryManager VkState::pGPUMemoryManager
