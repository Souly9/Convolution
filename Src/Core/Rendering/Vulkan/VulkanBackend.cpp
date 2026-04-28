#include "Core/Global/FrameGlobals.h"
#include "vulkan/vulkan_core.h"
#define GLFW_INCLUDE_VULKAN
#include "VulkanBackend.h"
#include "Core/Events/EventSystem.h"
#include "Core/Global/State/ApplicationState.h"
#undef max
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Rendering/Core/Attachment.h"
#include "Core/Rendering/Core/Nvidia/StreamlineManager.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/RenderLayer.h"
#include "Core/Rendering/Vulkan/BackendDefines.h"
#include "VkGlobals.h"
#include "VkTextureManager.h"
#include "Utils/VkEnumHelpers.h"
#include <EASTL/set.h>
#include <GLFW/glfw3.h>

PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectName = VK_NULL_HANDLE;
PFN_vkCmdSetCheckpointNV vkCmdSetCheckpoint = VK_NULL_HANDLE;
PFN_vkCmdBeginDebugUtilsLabelEXT vkBeginDebugUtilsLabel = VK_NULL_HANDLE;
PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabel = VK_NULL_HANDLE;

namespace
{
bool IsSrgbSwapchainFormat(VkFormat format)
{
    return format == VK_FORMAT_B8G8R8A8_SRGB || format == VK_FORMAT_R8G8B8A8_SRGB;
}

bool HasExtension(const stltype::vector<const char*>& extensions, const char* name)
{
    for (const char* existing : extensions)
    {
        if (strcmp(existing, name) == 0)
            return true;
    }
    return false;
}

bool HasExtension(const stltype::vector<VkExtensionProperties>& extensions, const char* name)
{
    for (const auto& existing : extensions)
    {
        if (strcmp(existing.extensionName, name) == 0)
            return true;
    }
    return false;
}

void RemoveExtension(stltype::vector<const char*>& extensions, const char* name)
{
    auto it = extensions.begin();
    while (it != extensions.end())
    {
        if (strcmp(*it, name) == 0)
            it = extensions.erase(it);
        else
            ++it;
    }
}

void NormalizeBufferDeviceAddressExtensions(stltype::vector<const char*>& extensions)
{
    // We use VkPhysicalDeviceVulkan12Features::bufferDeviceAddress, so EXT/KHR BDA extensions
    // must not be enabled together (and EXT must not be enabled at all in this path).
    RemoveExtension(extensions, VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
}

mathstl::Vector2 CalculateRenderResolution(const mathstl::Vector2& swapchainResolution, u32 upscalingPercentage)
{
    const f32 scale = static_cast<f32>(upscalingPercentage) / 100.0f;
    return {static_cast<f32>(stltype::max(1u, static_cast<u32>(swapchainResolution.x * scale))),
            static_cast<f32>(stltype::max(1u, static_cast<u32>(swapchainResolution.y * scale)))};
}
} // namespace

bool RenderBackendImpl<Vulkan>::Init(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title)
{
    m_dlssSupportAvailable = Nvidia::StreamlineManager::EarlyInit();

    if (!CreateInstance(screenWidth, screenHeight, title))
    {
        DEBUG_ASSERT(false && "Vulkan Instance couldn't be created!");
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

    if (m_dlssSupportAvailable && !DeviceSupportsDLSSRequirements(m_physicalDevice))
    {
        DEBUG_LOG_WARN("Selected Vulkan device does not satisfy Streamline DLSS requirements. DLSS disabled.");
        m_dlssSupportAvailable = false;
        Nvidia::StreamlineManager::Shutdown();
    }

    if (!CreateLogicalDevice())
    {
        DEBUG_LOG_ERR("Vulkan Logical Device couldn't be picked!");
        return false;
    }
    DEBUG_LOG("Vulkan physical and logical device created!");

    VkGlobals::SetLogicalDevice(m_logicalDevice);
    vkCmdSetCheckpoint =
        reinterpret_cast<PFN_vkCmdSetCheckpointNV>(vkGetDeviceProcAddr(VK_LOGICAL_DEVICE, "vkCmdSetCheckpointNV"));
    
    vkSetDebugUtilsObjectName =
        reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetInstanceProcAddr(m_instance, "vkSetDebugUtilsObjectNameEXT"));
    vkBeginDebugUtilsLabel = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(m_instance, "vkCmdBeginDebugUtilsLabelEXT"));
    vkCmdEndDebugUtilsLabel = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
        vkGetInstanceProcAddr(m_instance, "vkCmdEndDebugUtilsLabelEXT"));

    UpdateGlobals();
    bool dlssSupported = false;
    if (m_dlssSupportAvailable)
    {
        dlssSupported = Nvidia::StreamlineManager::Init();
        if (!dlssSupported)
        {
            DEBUG_LOG_WARN("Streamline DLSS initialization failed. DLSS disabled.");
            m_dlssSupportAvailable = false;
        }
    }
    PublishDLSSSupport(dlssSupported);

    {
        DEBUG_LOG("Creating Swapchain!");
        if (!CreateSwapChain(VK_NULL_HANDLE))
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

bool RenderBackendImpl<Vulkan>::RecreateSwapChain()
{
    vkDeviceWaitIdle(m_logicalDevice);

    VkSwapchainKHR oldSwapchain = m_swapChain;

    if (!CreateSwapChain(oldSwapchain))
    {
        DEBUG_LOG_ERR("Vulkan swapchain couldn't be recreated!");
        return false;
    }

    if (oldSwapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_logicalDevice, oldSwapchain, VulkanAllocator());
    }

    CreateSwapChainImages();
    UpdateGlobals();

    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    g_pEventSystem->OnSwapchainRecreated({
        .swapchainResolution = m_swapChainExtent,
        .renderResolution = CalculateRenderResolution(m_swapChainExtent, renderState.upscalingPercentage),
        .swapchainWasRecreated = true,
    });

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
    Nvidia::StreamlineManager::Shutdown();
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
    CreateDebugReportCallback =
        (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugReportCallbackEXT");

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;
    createInfo.pUserData = nullptr; // Optional

    if (vkCreateDebugUtilsMessengerEXT(m_instance, &createInfo, VulkanAllocator(), &m_debugMessenger) != VK_SUCCESS)
    {
        DEBUG_LOG_ERR("Failed to set up debug messenger!");
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL
RenderBackendImpl<Vulkan>::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                         VkDebugUtilsMessageTypeFlagsEXT messageType,
                                         const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                         void* pUserData)
{
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    {
        DEBUG_LOG(stltype::string(pCallbackData->pMessage));
        return VK_FALSE;
    }
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

    DEBUG_ASSERT(vkEnumerateInstanceVersion(&maxSupportedAPI) == VK_SUCCESS &&
                 "This application requires at least vulkan 1.1 support!");

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

    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    stltype::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

    sl::FeatureRequirements dlssRequirements{};
    if (m_dlssSupportAvailable && Nvidia::StreamlineManager::GetDLSSFeatureRequirements(dlssRequirements))
    {
        bool hasAllDLSSInstanceExtensions = true;
        for (u32 i = 0; i < dlssRequirements.vkNumInstanceExtensions; ++i)
        {
            const char* ext = dlssRequirements.vkInstanceExtensions[i];
            if (!HasExtension(extensions, ext))
            {
                hasAllDLSSInstanceExtensions = false;
                DEBUG_LOG_WARN(stltype::string("Missing Streamline instance extension: ") + ext);
                continue;
            }
            if (!HasExtension(instanceExtensions, ext))
                instanceExtensions.push_back(ext);
        }
        if (!hasAllDLSSInstanceExtensions)
        {
            DEBUG_LOG_WARN("Required Streamline instance extensions are unavailable. DLSS disabled.");
            m_dlssSupportAvailable = false;
            Nvidia::StreamlineManager::Shutdown();
        }
    }

    bool hasValidationFeaturesExt = false;
    for (const auto& ext : extensions)
    {
        if (strcmp(ext.extensionName, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME) == 0)
        {
            hasValidationFeaturesExt = true;
            break;
        }
    }

    if (hasValidationFeaturesExt)
    {
        instanceExtensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
        DEBUG_LOG("VK_EXT_validation_features available.");
    }
    else
    {
        DEBUG_LOG_WARN("VK_EXT_validation_features not available. Shader debug printf may be unavailable.");
    }

    createInfo.enabledExtensionCount = instanceExtensions.size();
    createInfo.ppEnabledExtensionNames = instanceExtensions.data();
    createInfo.enabledLayerCount = 0;

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

#ifdef CONV_DEBUG
    VkValidationFeatureEnableEXT enabledValidationFeatures[] = {
        VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT, VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT};
    VkValidationFeaturesEXT validationFeatures{};
    validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    validationFeatures.enabledValidationFeatureCount = 2;
    validationFeatures.pEnabledValidationFeatures = enabledValidationFeatures;

    if (hasValidationFeaturesExt)
    {
        createInfo.pNext = &validationFeatures;
        DEBUG_LOG("Validation feature VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT enabled.");
    }
#endif

    return vkCreateInstance(&createInfo, VulkanAllocator(), &m_instance) == VK_SUCCESS;
}

bool RenderBackendImpl<Vulkan>::CreateLogicalDevice()
{
    sl::FeatureRequirements dlssRequirements{};
    const bool hasDLSSRequirements =
        m_dlssSupportAvailable && Nvidia::StreamlineManager::GetDLSSFeatureRequirements(dlssRequirements);

    const u32 hostGraphicsQueueCount = 1;
    const u32 hostComputeQueueCount = 1;
    const u32 slGraphicsQueueCount = hasDLSSRequirements ? dlssRequirements.vkNumGraphicsQueuesRequired : 0;
    const u32 slComputeQueueCount = hasDLSSRequirements ? dlssRequirements.vkNumComputeQueuesRequired : 0;
    u32 finalGraphicsQueueCount = hostGraphicsQueueCount;
    u32 finalComputeQueueCount = hostComputeQueueCount;

    stltype::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    stltype::vector<stltype::vector<float>> queuePriorities;
    stltype::set<u32> uniqueQueueFamilies = {m_indices.graphicsFamily.value(),
                                             m_indices.presentFamily.value(),
                                             m_indices.transferFamily.value(),
                                             m_indices.computeFamily.value()};
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, nullptr);
    stltype::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyCount, queueFamilyProperties.data());
    queueCreateInfos.reserve(uniqueQueueFamilies.size());
    queuePriorities.reserve(uniqueQueueFamilies.size());

    for (u32 queueFamily : uniqueQueueFamilies)
    {
        u32 queueCount = 1;
        if (queueFamily == m_indices.graphicsFamily.value())
        {
            const u32 requiredGraphicsQueues = hostGraphicsQueueCount + slGraphicsQueueCount;
            queueCount = queueCount > requiredGraphicsQueues ? queueCount : requiredGraphicsQueues;
        }
        if (queueFamily == m_indices.computeFamily.value())
        {
            const u32 requiredComputeQueues = hostComputeQueueCount + slComputeQueueCount;
            queueCount = queueCount > requiredComputeQueues ? queueCount : requiredComputeQueues;
        }

        if (queueFamily < queueFamilyProperties.size())
        {
            const u32 availableQueueCount = queueFamilyProperties[queueFamily].queueCount;
            if (queueCount > availableQueueCount)
            {
                DEBUG_LOG_WARN("Vulkan queue family has fewer queues than requested by host/SL requirements");
                queueCount = availableQueueCount;
            }
        }

        queuePriorities.emplace_back(queueCount, 1.0f);
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = queueCount;
        queueCreateInfo.pQueuePriorities = queuePriorities.back().data();
        queueCreateInfos.push_back(queueCreateInfo);

        if (queueFamily == m_indices.graphicsFamily.value())
            finalGraphicsQueueCount = queueCount;
        if (queueFamily == m_indices.computeFamily.value())
            finalComputeQueueCount = queueCount;
    }

    if (finalGraphicsQueueCount < hostGraphicsQueueCount + slGraphicsQueueCount ||
        finalComputeQueueCount < hostComputeQueueCount + slComputeQueueCount)
    {
        DEBUG_LOG_WARN("Insufficient Vulkan queues for full Streamline DLSS queue requirements");
    }

    if (hasDLSSRequirements)
    {
        Nvidia::StreamlineManager::SetVulkanQueueStartIndices(
            finalGraphicsQueueCount > hostGraphicsQueueCount ? hostGraphicsQueueCount : 0,
            finalComputeQueueCount > hostComputeQueueCount ? hostComputeQueueCount : 0);
    }

    uint32_t deviceExtensionCount;
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &deviceExtensionCount, nullptr);
    stltype::vector<VkExtensionProperties> availableDeviceExtensions(deviceExtensionCount);
    vkEnumerateDeviceExtensionProperties(m_physicalDevice, nullptr, &deviceExtensionCount, availableDeviceExtensions.data());

    bool hasPageableMemory = false, hasMemoryPriority = false, hasAftermath = false;
    stltype::vector<const char*> enabledExtensions = g_requiredDeviceExtensions;
    if (hasDLSSRequirements)
    {
        for (u32 i = 0; i < dlssRequirements.vkNumDeviceExtensions; ++i)
        {
            const char* ext = dlssRequirements.vkDeviceExtensions[i];
            if (!HasExtension(enabledExtensions, ext))
                enabledExtensions.push_back(ext);
        }
    }
    for (const auto& avail : availableDeviceExtensions)
    {
        if (strcmp(avail.extensionName, VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME) == 0) hasPageableMemory = true;
        for (const char* opt : g_optionalDeviceExtensions)
        {
            if (strcmp(opt, avail.extensionName) == 0)
            {
                if (!HasExtension(enabledExtensions, opt))
                    enabledExtensions.push_back(opt);
                if (strcmp(opt, VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME) == 0) hasAftermath = true;
                if (strcmp(opt, VK_EXT_MEMORY_PRIORITY_EXTENSION_NAME) == 0) hasMemoryPriority = true;
            }
        }
    }
    if (hasPageableMemory) enabledExtensions.push_back(VK_EXT_PAGEABLE_DEVICE_LOCAL_MEMORY_EXTENSION_NAME);
    NormalizeBufferDeviceAddressExtensions(enabledExtensions);

    // Feature structs
    VkPhysicalDeviceMemoryPriorityFeaturesEXT memFeat{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PRIORITY_FEATURES_EXT, nullptr, VK_TRUE};
    VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT pageFeat{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT, 
                                                                  hasMemoryPriority ? &memFeat : nullptr, VK_TRUE};
    VkPhysicalDeviceVulkan14Features features14{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES, hasPageableMemory ? (void*)&pageFeat : (hasMemoryPriority ? (void*)&memFeat : nullptr), VK_TRUE};
    VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, &features14, VK_TRUE, VK_TRUE, VK_TRUE, VK_TRUE};
    VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, &features13, VK_TRUE, VK_TRUE, 0, VK_TRUE, VK_TRUE, VK_TRUE, VK_TRUE, VK_TRUE, VK_TRUE, VK_TRUE, VK_TRUE, 0, 0, VK_TRUE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, VK_TRUE};
    VkPhysicalDeviceVulkan11Features features11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, &features12, VK_TRUE};
    VkPhysicalDeviceFeatures2 deviceFeatures2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &features11};
    vkGetPhysicalDeviceFeatures2(m_physicalDevice, &deviceFeatures2);
    deviceFeatures2.features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    createInfo.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = static_cast<u32>(enabledExtensions.size());
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();

    VkDeviceDiagnosticsConfigCreateInfoNV aftermathInfo{VK_STRUCTURE_TYPE_DEVICE_DIAGNOSTICS_CONFIG_CREATE_INFO_NV, &deviceFeatures2, 
        VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV | VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV | 
        VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV | VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_ERROR_REPORTING_BIT_NV};

    createInfo.pNext = hasAftermath ? (void*)&aftermathInfo : (void*)&deviceFeatures2;

    if (vkCreateDevice(m_physicalDevice, &createInfo, VulkanAllocator(), &m_logicalDevice) != VK_SUCCESS)
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

    // First pass: look for a discrete GPU
    for (const auto& device : devices)
    {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && IsDeviceSuitable(device))
        {
            m_physicalDevice = device;
            break;
        }
    }

    // Second pass: fallback to any suitable device if no discrete found
    if (m_physicalDevice == VK_NULL_HANDLE)
    {
        for (const auto& device : devices)
        {
            if (IsDeviceSuitable(device))
            {
                m_physicalDevice = device;
                break;
            }
        }
    }

    if (m_physicalDevice != VK_NULL_HANDLE)
    {
        DEBUG_ASSERT(g_pApplicationState != nullptr);

        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(m_physicalDevice, &deviceProperties);
        DEBUG_LOGF("Picked Vulkan physical device: {}", deviceProperties.deviceName);

        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);
        VkGlobals::SetPhysicalDeviceMemoryProperties(memProperties);

        stltype::string deviceName(deviceProperties.deviceName);
        g_pApplicationState->RegisterUpdateFunction([deviceName](ApplicationState& state)
                                                    { state.renderState.physicalRenderDeviceName = deviceName; });
        return true;
    }

    return false;
}

bool RenderBackendImpl<Vulkan>::IsDeviceSuitable(VkPhysicalDevice device)
{
    if (device == VK_NULL_HANDLE)
        return false;
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    stltype::string deviceName(deviceProperties.deviceName);
    DEBUG_LOG("Found Device: " + deviceName);

    m_indices = FindQueueFamilies(device);
    // Fallback for missing families (unified queues/RenderDoc)
    if (!m_indices.transferFamily.has_value()) m_indices.transferFamily = m_indices.graphicsFamily;
    if (!m_indices.computeFamily.has_value()) m_indices.computeFamily = m_indices.graphicsFamily;

    bool swapChainAdequate = false;
    const bool extensionsSupported = AreExtensionsSupported(device);
    if (extensionsSupported)
    {
        SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    bool isSuitable = m_indices.IsComplete() && swapChainAdequate && deviceFeatures.samplerAnisotropy;

    if (!isSuitable)
    {
        DEBUG_LOG_WARNF("Rejecting Vulkan device '{}': queuesComplete={}, extensionsSupported={}, swapchainAdequate={}, samplerAnisotropy={}",
                        deviceProperties.deviceName,
                        m_indices.IsComplete(),
                        extensionsSupported,
                        swapChainAdequate,
                        deviceFeatures.samplerAnisotropy == VK_TRUE);
        return false;
    }

    VkGlobals::SetPhysicalDeviceProperties(deviceProperties);
    return true;
}

bool RenderBackendImpl<Vulkan>::AreExtensionsSupported(VkPhysicalDevice device)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    stltype::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    stltype::set<stltype::string> requiredExtensions(g_requiredDeviceExtensions.begin(), g_requiredDeviceExtensions.end());
    // BDA is provided via Vulkan 1.2 features in this renderer; don't require extension variants.
    requiredExtensions.erase(VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    requiredExtensions.erase(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);

    for (const auto& extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    for (const auto& missing : requiredExtensions)
    {
        DEBUG_LOG_WARNF("Missing required Vulkan device extension: {}", missing.c_str());
    }

    return requiredExtensions.empty();
}

bool RenderBackendImpl<Vulkan>::DeviceSupportsDLSSRequirements(VkPhysicalDevice device) const
{
    if (device == VK_NULL_HANDLE)
        return false;

    sl::FeatureRequirements dlssRequirements{};
    if (!Nvidia::StreamlineManager::GetDLSSFeatureRequirements(dlssRequirements))
        return false;

    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    stltype::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    for (u32 i = 0; i < dlssRequirements.vkNumDeviceExtensions; ++i)
    {
        const char* ext = dlssRequirements.vkDeviceExtensions[i];
        if (!HasExtension(availableExtensions, ext))
        {
            DEBUG_LOG_WARN(stltype::string("Missing Streamline device extension: ") + ext);
            return false;
        }
    }

    return true;
}

void RenderBackendImpl<Vulkan>::PublishDLSSSupport(bool supported) const
{
    if (!g_pApplicationState)
        return;

    g_pApplicationState->RegisterUpdateFunction(
        [supported](ApplicationState& state)
        {
            state.renderState.dlssSupported = supported;
            if (!supported && state.renderState.aaType == AntialiasingType::DLSS)
            {
                state.renderState.aaType = AntialiasingType::TAA_SMAA;
            }
        });
}

RenderBackendImpl<Vulkan>::SwapChainSupportDetails RenderBackendImpl<Vulkan>::QuerySwapChainSupport(
    VkPhysicalDevice device)
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

VkSurfaceFormatKHR RenderBackendImpl<Vulkan>::ChooseSwapSurfaceFormat(
    const stltype::vector<VkSurfaceFormatKHR>& availableFormats)
{
    DEBUG_ASSERT(!availableFormats.empty());

    // Prefer common sRGB SDR swapchain formats with sRGB nonlinear presentation.
    VkFormat preferredSrgbFormats[] = {Conv(g_swapChainFormat)};

    for (VkFormat preferredFormat : preferredSrgbFormats)
    {
        for (const auto& availableFormat : availableFormats)
        {
            if (availableFormat.format == preferredFormat && availableFormat.colorSpace == SWAPCHAINCOLORSPACE)
            {
                return availableFormat;
            }
        }
    }

    DEBUG_LOG_ERR("No supported sRGB swapchain format found on this surface.");
    return availableFormats[0];
}

VkPresentModeKHR RenderBackendImpl<Vulkan>::ChooseSwapPresentMode(
    const stltype::vector<VkPresentModeKHR>& availablePresentModes)
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

DirectX::XMUINT2 RenderBackendImpl<Vulkan>::GetWindowFramebufferExtent() const
{
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(g_pWindowManager->GetWindow(), &width, &height);
    return {static_cast<uint32_t>(stltype::max(width, 1)),
            static_cast<uint32_t>(stltype::max(height, 1))};
}

DirectX::XMUINT2 RenderBackendImpl<Vulkan>::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != stltype::numeric_limits<u32>::max())
    {
        return {capabilities.currentExtent.width, capabilities.currentExtent.height};
    }

    DirectX::XMUINT2 actualExtent = GetWindowFramebufferExtent();
    actualExtent.x = stltype::clamp(actualExtent.x, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.y = stltype::clamp(actualExtent.y, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return actualExtent;
}

bool RenderBackendImpl<Vulkan>::CreateSwapChain(VkSwapchainKHR oldSwapchain)
{
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);

    VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
    DirectX::XMUINT2 extent = ChooseSwapExtent(swapChainSupport.capabilities);

    // requesting one more image than the minimum to avoid stalls
    u32 imageCount = (swapChainSupport.capabilities.minImageCount <= SWAPCHAIN_IMAGES &&
                      swapChainSupport.capabilities.maxImageCount >= SWAPCHAIN_IMAGES)
                         ? SWAPCHAIN_IMAGES
                         : swapChainSupport.capabilities.maxImageCount;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }
    DEBUG_LOGF("Creating swapchain with {} images", imageCount);
    // DEBUG_LOG("Creating swapchain with " + stltype::to_string(imageCount) + " images, width: " +
    // stltype::to_string(extent.x) + ", height: " + stltype::to_string(extent.y));

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = Conv(extent);
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // TODO: change this if not rendering directly to it

    uint32_t queueFamilyIndices[] = {m_indices.graphicsFamily.value(), m_indices.presentFamily.value()};

    if (m_indices.graphicsFamily != m_indices.presentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;     // Optional
        createInfo.pQueueFamilyIndices = nullptr; // Optional
    }
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = oldSwapchain;

    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(m_logicalDevice, &createInfo, nullptr, &newSwapchain) != VK_SUCCESS)
        return false;

    m_swapChain = newSwapchain;
    vkGetSwapchainImagesKHR(m_logicalDevice, m_swapChain, &imageCount, nullptr);
    m_swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(m_logicalDevice, m_swapChain, &imageCount, m_swapChainImages.data());

    FrameGlobals::SetSwapChainFormat(Conv(surfaceFormat.format));
    m_swapChainExtent = mathstl::Vector2(extent.x, extent.y);
    return true;
}

void RenderBackendImpl<Vulkan>::CreateSwapChainImages()
{
    const auto ex = DirectX::XMUINT3(m_swapChainExtent.x, m_swapChainExtent.y, 1);
    TextureInfoBase info{};
    info.extents = ex;

    g_pTexManager->GetSwapChainTextures().clear();

    for (auto& image : m_swapChainImages)
    {
        g_pTexManager->CreateSwapchainTextures({Conv(SWAPCHAIN_FORMAT), image}, info);
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
    FrameGlobals::SetSwapChainExtent(m_swapChainExtent);
    VkGlobals::SetMainSwapChain(m_swapChain);
    VkGlobals::SetPresentQueue(m_presentQueue);
    VkGlobals::SetGraphicsQueue(m_graphicsQueue);
    VkGlobals::SetQueueFamilyIndices(m_indices);
    VkGlobals::SetAllQueues(Queues{m_graphicsQueue, m_presentQueue, m_transferQueue, m_computeQueue});
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
        if (!m_indices.graphicsFamily.has_value() && (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT))
        {
            m_indices.graphicsFamily = i;
        }
        else if (!m_indices.transferFamily.has_value() && (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT))
        {
            m_indices.transferFamily = i;
        }
        else if (!m_indices.computeFamily.has_value() && (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT))
        {
            m_indices.computeFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
        if (presentSupport && !m_indices.presentFamily.has_value())
        {
            m_indices.presentFamily = i;
        }

        if (m_indices.IsComplete()) break;
        ++i;
    }
    return m_indices;
}

QueueFamilyIndices RenderBackendImpl<Vulkan>::GetQueueFamilies() const
{
    return m_indices;
}
