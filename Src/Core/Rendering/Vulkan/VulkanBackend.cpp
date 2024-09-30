#define GLFW_INCLUDE_VULKAN
#include <glfw3/glfw3.h>
#include <EASTL/set.h>
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/LogDefines.h"
#include "Core/Rendering/RenderLayer.h"
#include "Core/Rendering/Core/Attachment.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/Vulkan/BackendDefines.h"
#include "VulkanBackend.h"
#include "VkGlobals.h"
#include "VkTextureManager.h"
#include "Core/Global/GlobalVariables.h"

bool RenderBackendImpl<Vulkan>::Init(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title)
{
	if(!CreateInstance(screenWidth, screenHeight, title))
	{
		DEBUG_LOG_ERR("Vulkan Instance couldn't be created!");
		return false;
	}
	DEBUG_LOG("Vulkan Instance created!");
	if (!CreateSurface())
	{
		DEBUG_LOG_ERR("Vulkan surface couldn't be created!");
		return false;
	}
	if (!PickPhysicalDevice())
	{
		DEBUG_LOG_ERR("Vulkan Physical Device couldn't be picked!");
		return false;
	}

	if (!CreateLogicalDevice())
	{
		DEBUG_LOG_ERR("Vulkan Logical Device couldn't be picked!");
		return false;
	}
	DEBUG_LOG("Vulkan physical and logical device created!");

	VkGlobals::SetLogicalDevice(m_logicalDevice);
	{
		DEBUG_LOG("Creating Swapchain!");
		if (!CreateSwapChain())
		{
			DEBUG_LOG_ERR("Vulkan swapchain couldn't be created!");
			return false;
		}

		UpdateGlobals();
		CreateSwapChainImages();
	}

	UpdateGlobals();
	return true;
}

bool RenderBackendImpl<Vulkan>::Cleanup()
{
	vkDestroySwapchainKHR(m_logicalDevice, m_swapChain, VulkanAllocator());
	vkDestroySurfaceKHR(m_instance, m_surface, VulkanAllocator());
	m_swapChainTextures.clear();
	vkDestroyDevice(m_logicalDevice, VulkanAllocator());
	vkDestroyInstance(m_instance, VulkanAllocator());
	return true;
}

bool RenderBackendImpl<Vulkan>::AreValidationLayersAvailable(const stltype::vector<VkLayerProperties>& availableLayers)
{
	uint32_t available = 0;
	for (const auto& extension : availableLayers)
	{
		for (const auto& neededLayer : g_validationLayers)
		{
			if (strcmp(extension.layerName, neededLayer) == 0)
			{
				++available;
			}
		}
	}
	return available == g_validationLayers.size();
}

bool RenderBackendImpl<Vulkan>::CreateInstance(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title)
{
	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = title.data();
	appInfo.applicationVersion = VK_MAKE_API_VERSION(1, 0, 0, 0);
	appInfo.pEngineName = ENGINE_NAME.data();
	appInfo.engineVersion = VK_MAKE_VERSION(CONVOLUTION_MAJOR, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	// Using glfw because I sure as hell dont want to deal with the Windows API
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	createInfo.enabledExtensionCount = glfwExtensionCount;
	createInfo.ppEnabledExtensionNames = glfwExtensions;
	createInfo.enabledLayerCount = 0;

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	stltype::vector< VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	for (const auto& extension : extensions)
	{
		DEBUG_LOG(extension.extensionName);
	}

	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	stltype::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
	for (const auto& layer : availableLayers)
	{
		DEBUG_LOG(layer.layerName);
	}
	if (AreValidationLayersAvailable(availableLayers))
	{
		createInfo.enabledLayerCount = g_validationLayers.size();
		createInfo.ppEnabledLayerNames = g_validationLayers.data();
	}
	else
		DEBUG_LOG_ERR("Validation Layers not available!");

	return vkCreateInstance(&createInfo, VulkanAllocator(), &m_instance) == VK_SUCCESS;
}

bool RenderBackendImpl<Vulkan>::CreateLogicalDevice()
{
	stltype::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	stltype::set<u32> uniqueQueueFamilies = { m_indices.graphicsFamily.value(), m_indices.presentFamily.value(), m_indices.transferFamily.value(), m_indices.computeFamily.value() };

	float queuePriority = 1.0f;
	for (u32 queueFamily : uniqueQueueFamilies)
	{
		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	VkPhysicalDeviceFeatures deviceFeatures{};

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; 
	createInfo.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.enabledExtensionCount = static_cast<u32>(g_deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = g_deviceExtensions.data();

	createInfo.enabledLayerCount = 0; // No old vulkan versions here mister

	if (vkCreateDevice(m_physicalDevice, &createInfo, VulkanAllocator(), &m_logicalDevice)
		!= VK_SUCCESS)
		return false;

	 vkGetDeviceQueue(m_logicalDevice, m_indices.graphicsFamily.value(), 0, &m_graphicsQueue);
	 vkGetDeviceQueue(m_logicalDevice, m_indices.presentFamily.value(), 0, &m_presentQueue);
	 vkGetDeviceQueue(m_logicalDevice, m_indices.transferFamily.value(), 0, &m_transferQueue);
	 vkGetDeviceQueue(m_logicalDevice, m_indices.computeFamily.value(), 0, &m_computeQueue);

	 return true;
}

bool RenderBackendImpl<Vulkan>::PickPhysicalDevice()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		DEBUG_LOG_ERR("No Vulkan compatible GPU found!");
		return false;
	}
	stltype::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

	for (const auto& device : devices)
	{
		if (IsDeviceSuitable(device))
		{
			DEBUG_LOG("Picked this device");
			m_physicalDevice = device;
			return true;
		}
	}
	return false;
}

bool RenderBackendImpl<Vulkan>::IsDeviceSuitable(VkPhysicalDevice device)
{
	if(device == VK_NULL_HANDLE)
		return false;
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

	stltype::string deviceName(deviceProperties.deviceName);
	DEBUG_LOG("Found Device: " + deviceName);

	m_indices = FindQueueFamilies(device);
	bool swapChainAdequate = false;
	if (AreExtensionsSupported(device))
	{
		SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
		swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
		m_indices.IsComplete() && 
		swapChainAdequate;
}

bool RenderBackendImpl<Vulkan>::AreExtensionsSupported(VkPhysicalDevice device)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

	stltype::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

	stltype::set<stltype::string> requiredExtensions(g_deviceExtensions.begin(), g_deviceExtensions.end());

	for (const auto& extension : availableExtensions)
	{
		stltype::string extensionName(extension.extensionName);
		requiredExtensions.erase(extension.extensionName);
		DEBUG_LOG("Required extension" + extensionName + "is supported!");
	}

	return requiredExtensions.empty();
}

RenderBackendImpl<Vulkan>::SwapChainSupportDetails RenderBackendImpl<Vulkan>::QuerySwapChainSupport(VkPhysicalDevice device)
{
	SwapChainSupportDetails details;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities); 
	u32 formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);

	if (formatCount != 0)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
	}
	u32 presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);

	if (presentModeCount != 0)
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
	}
	return details;
}

VkSurfaceFormatKHR RenderBackendImpl<Vulkan>::ChooseSwapSurfaceFormat(const stltype::vector<VkSurfaceFormatKHR>& availableFormats)
{
	for (const auto& availableFormat : availableFormats)
	{
		if (availableFormat.format == SWAPCHAINFORMAT && availableFormat.colorSpace == SWAPCHAINCOLORSPACE)
		{
			return availableFormat;
		}
	}
	
	return availableFormats[0];
}

VkPresentModeKHR RenderBackendImpl<Vulkan>::ChooseSwapPresentMode(const stltype::vector<VkPresentModeKHR>& availablePresentModes)
{
	for (const auto& availablePresentMode : availablePresentModes)
	{
		if (availablePresentMode == SWAPCHAINPRESENTMODE)
		{
			return availablePresentMode;
		}
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

DirectX::XMUINT2 RenderBackendImpl<Vulkan>::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != stltype::numeric_limits<u32>::max())
	{
		DirectX::XMUINT2 extents = {
			static_cast<uint32_t>(capabilities.currentExtent.width),
			static_cast<uint32_t>(capabilities.currentExtent.height)
		};
		return extents;
	}
	else
	{
		int width, height;
		glfwGetFramebufferSize(g_pWindowManager->GetWindow(), &width, &height);

		DirectX::XMUINT2 actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.x = stltype::clamp(actualExtent.x, 
			capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.y = stltype::clamp(actualExtent.y,
			capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}

bool RenderBackendImpl<Vulkan>::CreateSwapChain()
{
	SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_physicalDevice);

	VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
	VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
	DirectX::XMUINT2 extent = ChooseSwapExtent(swapChainSupport.capabilities);

	u32 imageCount = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && 
		imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = Conv(extent);
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; //TODO: change this if not rendering directly to it

	uint32_t queueFamilyIndices[] = { m_indices.graphicsFamily.value(), m_indices.presentFamily.value() };

	if (m_indices.graphicsFamily != m_indices.presentFamily)
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0; // Optional
		createInfo.pQueueFamilyIndices = nullptr; // Optional
	}
	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;
	if (vkCreateSwapchainKHR(m_logicalDevice, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS)
		return false;

	vkGetSwapchainImagesKHR(m_logicalDevice, m_swapChain, &imageCount, nullptr);
	m_swapChainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(m_logicalDevice, m_swapChain, &imageCount, m_swapChainImages.data());
	
	m_swapChainImageFormat = surfaceFormat.format;
	m_swapChainExtent = extent;
	return true;
}

void RenderBackendImpl<Vulkan>::CreateSwapChainImages()
{
	m_swapChainTextures.reserve(m_swapChainImages.size());
	for (auto& image : m_swapChainImages)
	{
		const Texture tex = g_pTexManager->CreateFromImage({ m_swapChainImageFormat, image }, { VkGlobals::GetSwapChainExtent() });
		m_swapChainTextures.push_back(tex);
	}
}

bool RenderBackendImpl<Vulkan>::CreateSurface()
{
	return glfwCreateWindowSurface(m_instance, g_pWindowManager->GetWindow(), nullptr, &m_surface) == VK_SUCCESS;
}

bool RenderBackendImpl<Vulkan>::CreateGraphicsPipeline()
{
	return false;
}

void RenderBackendImpl<Vulkan>::UpdateGlobals() const
{
	VkGlobals::SetSwapChainImageFormat(m_swapChainImageFormat);
	VkGlobals::SetSwapChainExtent(m_swapChainExtent);
	VkGlobals::SetMainSwapChain(m_swapChain);
	VkGlobals::SetPresentQueue(m_presentQueue);
	VkGlobals::SetGraphicsQueue(m_graphicsQueue);
	VkGlobals::SetSwapChainImages(m_swapChainTextures);
	VkGlobals::SetQueueFamilyIndices(m_indices);
	VkGlobals::SetAllQueues(Queues{ m_graphicsQueue, m_presentQueue, m_transferQueue, m_computeQueue });
	VkGlobals::SetPhysicalDevice(m_physicalDevice);
}

QueueFamilyIndices RenderBackendImpl<Vulkan>::FindQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices m_indices;

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	stltype::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

	s32 i = 0;
	for (const auto& queueFamily : queueFamilies)
	{
		if (m_indices.graphicsFamily.has_value() == false && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			m_indices.graphicsFamily = i;
		}
		else if (m_indices.transferFamily.has_value() == false && queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT)
		{
			m_indices.transferFamily = i;
		}
		else if (m_indices.computeFamily.has_value() == false && queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
		{
			m_indices.computeFamily = i;
		}
		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
		if (presentSupport)
		{
			m_indices.presentFamily = i;
		}
		if (m_indices.IsComplete())
		{
			break;
		}
		++i;
	}
	return m_indices;
}

QueueFamilyIndices RenderBackendImpl<Vulkan>::GetQueueFamilies() const
{
	return m_indices;
}

const stltype::vector<Texture>& RenderBackendImpl<Vulkan>::GetSwapChainTextures() const
{
	return m_swapChainTextures;
}
