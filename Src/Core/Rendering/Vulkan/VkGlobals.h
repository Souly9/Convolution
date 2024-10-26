#pragma once
#include "Core/Global/GlobalDefines.h"
#include "VkTextureManager.h"
#include "VkGPUMemoryManager.h"

extern stltype::unique_ptr<GPUMemManager<Vulkan>> g_pGPUMemoryManager;
extern stltype::unique_ptr<VkTextureManager> g_pTexManager;

struct Queues
{
	VkQueue graphics;
	VkQueue present;
	VkQueue transfer;
	VkQueue compute;
};

// Isn't responsible for freeing or managing the globals, just provides access
class VkGlobals
{
public:
	static VkDevice GetLogicalDevice();
	static VkFormat GetSwapChainImageFormat();
	static DirectX::XMUINT2 GetSwapChainExtent();
	static f32 GetScreenAspectRatio();
	static VkSwapchainKHR GetMainSwapChain();
	static VkQueue GetPresentQueue();
	static VkQueue GetGraphicsQueue();
	static Queues GetAllQueues();
	static const stltype::vector<Texture*>& GetSwapChainImages();
	static QueueFamilyIndices GetQueueFamilyIndices();
	static VkPhysicalDevice GetPhysicalDevice();
	static const VkPhysicalDeviceProperties& GetPhysicalDeviceProperties();

	static void SetPhysicalDeviceProperties(const VkPhysicalDeviceProperties& physDeviceProps);
	static void SetLogicalDevice(VkDevice physDevice);
	static void SetSwapChainImageFormat(VkFormat physDevice);
	static void SetSwapChainExtent(const DirectX::XMUINT2& physDevice);
	static void SetMainSwapChain(const VkSwapchainKHR swapChain);
	static void SetPresentQueue(const VkQueue presentQueue);
	static void SetGraphicsQueue(const VkQueue graphicsQueue);
	static void SetAllQueues(const Queues& queues);
	static void SetSwapChainImages(const stltype::vector<Texture*>& images);
	static void SetQueueFamilyIndices(const QueueFamilyIndices& indices);
	static void SetPhysicalDevice(const VkPhysicalDevice& physDevice);
private:
	static stltype::vector<Texture*> s_swapChainImages;
	static QueueFamilyIndices s_indices;
	static VkDevice s_logicalDevice;
	static VkFormat s_swapChainImageFormat;
	static DirectX::XMUINT2 s_swapChainExtent;
	static VkSwapchainKHR s_mainSwapChain;
	static VkQueue s_presentQueue;
	static VkQueue s_graphicsQueue;
	static Queues s_queues;
	static VkPhysicalDevice s_physicalDevice;
	static VkPhysicalDeviceProperties s_physicalDeviceProperties;
};