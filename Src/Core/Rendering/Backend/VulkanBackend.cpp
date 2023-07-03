#include <glfw3/glfw3.h>
#include "Core/Global/GlobalDefines.h"
#include "BackendDefines.h"
#include "Core/Rendering/RenderInformation.h"
#include "Core/UI/UIData.h"
#include "VulkanBackend.h"

bool VulkanBackend::Init(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title)
{
	RenderInformation informations;
	if(!CreateInstance(screenWidth, screenHeight, title))
	{
		informations.m_backendErrors._errorsThisFrame.emplace_back("Vulkan Instance couldn't be created!");
		UIData::Get()->PushRenderInformation(stltype::move(informations));
		return false;
	}
	return true;
}

bool VulkanBackend::Cleanup()
{
	vkDestroyInstance(m_instance, VulkanAllocator());
	return true;
}

bool VulkanBackend::AreValidationLayersAvailable(const stltype::vector<stltype::string>& availableLayers)
{
	uint32_t available = 0;
	for (const auto& extension : availableLayers)
	{
		for (const auto& neededLayer : validationLayers)
		{
			if (strcmp(extension.data(), neededLayer) == 0)
			{
				++available;
			}
		}
	}
	return available == validationLayers.size();
}

bool VulkanBackend::CreateInstance(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title)
{
	RenderInformation informations;

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

	informations.m_generalInfo._availableExtensions.reserve(extensionCount);
	for (const auto& extension : extensions)
	{
		informations.m_generalInfo._availableExtensions.emplace_back(extension.extensionName);
	}

	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	stltype::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
	for (const auto& layer : availableLayers)
	{
		informations.m_generalInfo._availableLayers.emplace_back(layer.layerName);
	}
	if (AreValidationLayersAvailable(informations.m_generalInfo._availableLayers))
	{
		createInfo.enabledLayerCount = validationLayers.size();
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
		informations.m_backendErrors._persistentErrors.emplace_back("Validation Layers not available!");

	UIData::Get()->PushRenderInformation(stltype::move(informations));
	return vkCreateInstance(&createInfo, VulkanAllocator(), &m_instance) == VK_SUCCESS;
}
