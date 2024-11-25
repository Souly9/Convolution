#pragma once
#include "Core/Global/GlobalDefines.h"
#include "VkTextureManager.h"
#include "VkGPUMemoryManager.h"

extern stltype::unique_ptr<GPUMemManager<Vulkan>> g_pGPUMemoryManager;

struct Queues
{
	VkQueue graphics;
	VkQueue present;
	VkQueue transfer;
	VkQueue compute;
};

struct VulkanContext
{
	VkInstance            Instance;
	VkPhysicalDevice      PhysicalDevice;
	VkDevice              Device;   
	VkSampleCountFlagBits MSAASamples;    
};

// Isn't responsible for freeing or managing the globals, just provides access
class VkGlobals
{
public:
	static VkDevice GetLogicalDevice();
	static VkFormat GetSwapChainImageFormat();
	static VkSwapchainKHR GetMainSwapChain();
	static VkQueue GetPresentQueue();
	static VkQueue GetGraphicsQueue();
	static Queues GetAllQueues();
	static const stltype::vector<Texture*>& GetSwapChainImages();
	static QueueFamilyIndices GetQueueFamilyIndices();
	static VkPhysicalDevice GetPhysicalDevice();
	static const VkPhysicalDeviceProperties& GetPhysicalDeviceProperties();
	static Texture* GetDepthStencilBuffer();
	static void SetContext(const VulkanContext& context);

	static void SetPhysicalDeviceProperties(const VkPhysicalDeviceProperties& physDeviceProps);
	static void SetLogicalDevice(VkDevice physDevice);
	static void SetSwapChainImageFormat(VkFormat physDevice);
	static void SetMainSwapChain(const VkSwapchainKHR swapChain);
	static void SetPresentQueue(const VkQueue presentQueue);
	static void SetGraphicsQueue(const VkQueue graphicsQueue);
	static void SetAllQueues(const Queues& queues);
	static void SetSwapChainImages(const stltype::vector<Texture*>& images);
	static void SetQueueFamilyIndices(const QueueFamilyIndices& indices);
	static void SetPhysicalDevice(const VkPhysicalDevice& physDevice);;
	static void SetDepthStencilBuffer(Texture* pDepthTex);
	static const VulkanContext& GetContext();
private:
	static stltype::vector<Texture*> s_swapChainImages;
	static Texture* s_pDepthStencilBuffer;
	static QueueFamilyIndices s_indices;
	static VkDevice s_logicalDevice;
	static VkFormat s_swapChainImageFormat;
	static VkSwapchainKHR s_mainSwapChain;
	static VkQueue s_presentQueue;
	static VkQueue s_graphicsQueue;
	static Queues s_queues;
	static VkPhysicalDevice s_physicalDevice;
	static VkPhysicalDeviceProperties s_physicalDeviceProperties;
	static VulkanContext s_context;
};