#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
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
		DEBUG_ASSERT(false, "Vulkan Instance couldn't be created!");
		return false;
	}
	DEBUG_LOG("Vulkan Instance created!");
#ifdef CONV_DEBUG
	CreateDebugMessenger();
#endif
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
#ifdef CONV_DEBUG
	vkDestroyDebugUtilsMessengerEXT(m_instance, VulkanAllocator(), &m_debugMessenger);
#endif
	vkDestroySwapchainKHR(m_logicalDevice, m_swapChain, VulkanAllocator());
	vkDestroySurfaceKHR(m_instance, m_surface, VulkanAllocator());
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

void RenderBackendImpl<Vulkan>::CreateDebugMessenger()
{
	PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback = VK_NULL_HANDLE;
	CreateDebugReportCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugReportCallbackEXT");

	vkSetDebugUtilsObjectName = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(m_instance, "vkSetDebugUtilsObjectNameEXT"); 
	vkBeginDebugUtilsLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(vkGetInstanceProcAddr(m_instance, "vkCmdBeginDebugUtilsLabelEXT"));
	vkCmdEndDebugUtilsLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(vkGetInstanceProcAddr(m_instance, "vkCmdEndDebugUtilsLabelEXT"));

	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
	createInfo.pfnUserCallback = DebugCallback;
	createInfo.pUserData = nullptr; // Optional

	if (vkCreateDebugUtilsMessengerEXT(m_instance, &createInfo, VulkanAllocator(), &m_debugMessenger) != VK_SUCCESS)
	{
		DEBUG_LOG_ERR("Failed to set up debug messenger!");
	}
}

VKAPI_ATTR VkBool32 VKAPI_CALL RenderBackendImpl<Vulkan>::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
	{
		DEBUG_LOG(stltype::string(pCallbackData->pMessage));
		return VK_FALSE;
	}
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
	{
		DEBUG_LOG_WARN(stltype::string(pCallbackData->pMessage));
		return VK_FALSE;
	}
	if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	{
		DEBUG_LOG_ERR(stltype::string(pCallbackData->pMessage));
		return VK_TRUE;
	}
	return VK_FALSE;
}

bool RenderBackendImpl<Vulkan>::CreateInstance(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title)
{
	u32 maxSupportedAPI;

	DEBUG_ASSERT(vkEnumerateInstanceVersion(&maxSupportedAPI) == VK_SUCCESS, "This application requires at least vulkan 1.1 support!");

	VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = title.data();
	appInfo.applicationVersion = maxSupportedAPI;
	appInfo.pEngineName = ENGINE_NAME.data();
	appInfo.engineVersion = CONV_MIN_VULKAN_VERSION;
	appInfo.apiVersion = VK_API_LEVEL_MIN;

	VkInstanceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	// Using glfw because I sure as hell dont want to deal with the Windows API
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;

	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	auto instanceExtensions = stltype::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);
	instanceExtensions.insert(instanceExtensions.end(), g_instanceExtensions.begin(), g_instanceExtensions.end());

	createInfo.enabledExtensionCount = instanceExtensions.size();
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();
	createInfo.enabledLayerCount = 0;

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	stltype::vector< VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	stltype::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	if (AreValidationLayersAvailable(availableLayers))
	{
		createInfo.enabledLayerCount = g_validationLayers.size();
		createInfo.ppEnabledLayerNames = g_validationLayers.data();
	}
	else
	{
		DEBUG_ASSERT(false);
		DEBUG_LOG_ERR("Validation Layers not available!");
	}

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

	// Enabling bindless textures
	VkPhysicalDeviceDescriptorIndexingFeatures indexingFeaturesBindlessTextures{};
	indexingFeaturesBindlessTextures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	indexingFeaturesBindlessTextures.descriptorBindingPartiallyBound = VK_TRUE;
	indexingFeaturesBindlessTextures.runtimeDescriptorArray = VK_TRUE;
	indexingFeaturesBindlessTextures.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	indexingFeaturesBindlessTextures.descriptorBindingVariableDescriptorCount = VK_TRUE;
	indexingFeaturesBindlessTextures.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
	indexingFeaturesBindlessTextures.pNext = nullptr;

	// VK 1.3 features
	VkPhysicalDeviceVulkan13Features features13{};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.dynamicRendering = VK_TRUE;
	features13.synchronization2 = VK_TRUE;
	features13.pNext = &indexingFeaturesBindlessTextures;

	// VK 1.1 features
	VkPhysicalDeviceVulkan11Features features11{};
	features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	features11.pNext = &features13;

	VkDeviceCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size());
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.enabledExtensionCount = static_cast<u32>(g_deviceExtensions.size());
	createInfo.ppEnabledExtensionNames = g_deviceExtensions.data();
	createInfo.pEnabledFeatures = nullptr;

	VkPhysicalDeviceFeatures2 deviceFeatures2{};
	deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	vkGetPhysicalDeviceFeatures2(m_physicalDevice, &deviceFeatures2);
	deviceFeatures2.pNext = &features11;


	// Set up device creation info for Aftermath feature flag configuration.
	VkDeviceDiagnosticsConfigFlagsNV aftermathFlags =
		VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV |  // Enable automatic call stack checkpoints.
		VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV |      // Enable tracking of resources.
		VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV |      // Generate debug information for shaders.
		VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_ERROR_REPORTING_BIT_NV;  // Enable additional runtime shader error reporting.

	VkDeviceDiagnosticsConfigCreateInfoNV aftermathInfo = {};
	aftermathInfo.sType = VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV;
	aftermathInfo.flags = aftermathFlags;
	aftermathInfo.pNext = &deviceFeatures2;

	createInfo.pNext = &aftermathInfo;


	createInfo.enabledLayerCount = 0; // No old vulkan versions here mister

	if (vkCreateDevice(m_physicalDevice, &createInfo, VulkanAllocator(), &m_logicalDevice)
		!= VK_SUCCESS)
		return false;

	vkGetDeviceQueue(m_logicalDevice, m_indices.graphicsFamily.value(), 0, &m_graphicsQueue);
	vkGetDeviceQueue(m_logicalDevice, m_indices.presentFamily.value(), 0, &m_presentQueue);
	vkGetDeviceQueue(m_logicalDevice, m_indices.transferFamily.value(), 0, &m_transferQueue);
	vkGetDeviceQueue(m_logicalDevice, m_indices.computeFamily.value(), 0, &m_computeQueue);

	vkCmdSetCheckpoint = (PFN_vkCmdSetCheckpointNV)vkGetDeviceProcAddr(m_logicalDevice, "vkCmdSetCheckpointNV");
	DEBUG_ASSERT(vkCmdSetCheckpoint != VK_NULL_HANDLE);
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
			DEBUG_ASSERT(g_pApplicationState != nullptr);

			m_physicalDevice = device;
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(m_physicalDevice, &deviceProperties);

			stltype::string deviceName(deviceProperties.deviceName);
			g_pApplicationState->RegisterUpdateFunction([deviceName](ApplicationState& state) { state.renderState.physicalRenderDeviceName = deviceName; });
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

	bool isSuitable = deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
		m_indices.IsComplete() &&
		swapChainAdequate && deviceFeatures.samplerAnisotropy;

	if (!isSuitable)
		return false;

	VkGlobals::SetPhysicalDeviceProperties(deviceProperties);
	return true ;
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

	// requesting one more image than the minimum to avoid stalls
	u32 imageCount = (swapChainSupport.capabilities.minImageCount <= SWAPCHAIN_IMAGES && swapChainSupport.capabilities.maxImageCount >= SWAPCHAIN_IMAGES) ?
		SWAPCHAIN_IMAGES :
		swapChainSupport.capabilities.maxImageCount;
	if (swapChainSupport.capabilities.maxImageCount > 0 && 
		imageCount > swapChainSupport.capabilities.maxImageCount)
	{
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}
	DEBUG_LOGF("Creating swapchain with {} images", imageCount);
	//DEBUG_LOG("Creating swapchain with " + stltype::to_string(imageCount) + " images, width: " + stltype::to_string(extent.x) + ", height: " + stltype::to_string(extent.y));

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
	const auto ex = DirectX::XMUINT3(FrameGlobals::GetSwapChainExtent().x, FrameGlobals::GetSwapChainExtent().y, 0);
	TextureInfoBase info{};
	info.extents = ex;
	for (auto& image : m_swapChainImages)
	{
		g_pTexManager->CreateSwapchainTextures({ m_swapChainImageFormat, image }, info);
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
	FrameGlobals::SetSwapChainExtent(m_swapChainExtent);
	VkGlobals::SetMainSwapChain(m_swapChain);
	VkGlobals::SetPresentQueue(m_presentQueue);
	VkGlobals::SetGraphicsQueue(m_graphicsQueue);
	VkGlobals::SetQueueFamilyIndices(m_indices);
	VkGlobals::SetAllQueues(Queues{ m_graphicsQueue, m_presentQueue, m_transferQueue, m_computeQueue });
	VkGlobals::SetPhysicalDevice(m_physicalDevice);
	VulkanContext ctx;
	ctx.Device = m_logicalDevice;
	ctx.Instance = m_instance;
	ctx.PhysicalDevice = m_physicalDevice;
	ctx.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	VkGlobals::SetContext(ctx);
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
