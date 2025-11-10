#pragma once
#include "Core/Global/GlobalDefines.h"
#include <vulkan/vulkan.h>
#include "Core/Rendering/Backend/RenderBackend.h"
#include "Core/Rendering/Vulkan/VkPipeline.h"
#include "Core/Rendering/Vulkan/VkTexture.h"

template<>
class RenderBackendImpl<Vulkan>
{
public:
	virtual ~RenderBackendImpl() = default;
	bool Init(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title);

	bool Cleanup();

	bool AreValidationLayersAvailable(const stltype::vector<VkLayerProperties>& availableExtensions);

	void CreateDebugMessenger();

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		stltype::vector<VkSurfaceFormatKHR> formats;
		stltype::vector<VkPresentModeKHR> presentModes;
	};
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
	QueueFamilyIndices GetQueueFamilies() const;

	VkInstance GetInstance() const { return m_instance; }
	VkDevice GetLogicalDevice() const { return m_logicalDevice; }
	VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
	VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }

	VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pMessenger)
	{
		auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			func(instance, pCreateInfo, pAllocator, pMessenger);
		}
		return VK_SUCCESS;
	}
	VKAPI_ATTR VkResult VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pMessenger)
	{
		auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
		if (func != nullptr)
		{
			func(instance, *pMessenger, pAllocator);
		}
		return VK_SUCCESS;
	}

private:
	bool CreateInstance(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title);
	bool CreateLogicalDevice();
	bool PickPhysicalDevice();
	bool IsDeviceSuitable(VkPhysicalDevice device);
	bool AreExtensionsSupported(VkPhysicalDevice device);

	void CreateAndDistributeDepthBuffer();
	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const stltype::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR ChooseSwapPresentMode(const stltype::vector<VkPresentModeKHR>& availablePresentModes);
	DirectX::XMUINT2 ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	bool CreateSwapChain();
	void CreateSwapChainImages();

	/// @brief Creates the surface with glfw to interface with the window.
	/// @return Whether the surface was created.
	bool CreateSurface();

	bool CreateGraphicsPipeline();

	void UpdateGlobals() const;

	VkDebugUtilsMessengerEXT m_debugMessenger;
	VkInstance m_instance{ VK_NULL_HANDLE };
	VkDevice m_logicalDevice{ VK_NULL_HANDLE };
	VkPhysicalDevice m_physicalDevice{ VK_NULL_HANDLE };
	VkQueue m_graphicsQueue;
	VkSurfaceKHR m_surface{ VK_NULL_HANDLE }; ///< Surface that establishes connection to the glfw window.
	VkQueue m_presentQueue;
	VkQueue m_transferQueue;
	VkQueue m_computeQueue;
	QueueFamilyIndices m_indices;
	VkSwapchainKHR m_swapChain{ VK_NULL_HANDLE };
	stltype::vector<VkImage> m_swapChainImages;
	VkFormat m_swapChainImageFormat;
	mathstl::Vector2 m_swapChainExtent;
};