#pragma once
#include <vulkan/vulkan.h>
#include "Core/Rendering/Backend/RenderBackend.h"
#include "Core/Rendering/Vulkan/VkTexture.h"

template<>
class RenderBackend<Vulkan>
{
public:
	virtual ~RenderBackend() = default;
	bool Init(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title);

	bool Cleanup();

	bool AreValidationLayersAvailable(const stltype::vector<VkLayerProperties>& availableExtensions);

	void ErrorCallback();

	struct QueueFamilyIndices
	{
		stltype::optional<u32> graphicsFamily;
		stltype::optional<u32> presentFamily;

		bool IsComplete()
		{
			return graphicsFamily.has_value() && presentFamily.has_value();
		}
	}; 
	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		stltype::vector<VkSurfaceFormatKHR> formats;
		stltype::vector<VkPresentModeKHR> presentModes;
	};
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);

private:
	bool CreateInstance(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title);
	bool CreateLogicalDevice();
	bool PickPhysicalDevice();
	bool IsDeviceSuitable(VkPhysicalDevice device);
	bool AreExtensionsSupported(VkPhysicalDevice device);

	SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
	VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const stltype::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR ChooseSwapPresentMode(const stltype::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	bool CreateSwapChain();
	void CreateSwapChainImages();

	/// @brief Creates the surface with glfw to interface with the window.
	/// @return Whether the surface was created.
	bool CreateSurface();

	bool CreateGraphicsPipeline();

	void UpdateGlobals() const;

	VkInstance m_instance;
	VkDevice m_logicalDevice;
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkQueue m_graphicsQueue;
	VkSurfaceKHR m_surface; ///< Surface that establishes connection to the glfw window.
	VkQueue m_presentQueue;
	QueueFamilyIndices m_indices;
	VkSwapchainKHR m_swapChain;
	stltype::vector<VkImage> m_swapChainImages;
	VkFormat m_swapChainImageFormat;
	VkExtent2D m_swapChainExtent;
	stltype::vector<TexImpl<Vulkan>> m_swapChainTextures;
};