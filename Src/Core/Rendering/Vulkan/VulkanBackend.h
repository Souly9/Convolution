#pragma once
#include "Core/Engine.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Backend/BackendForwardDecls.h"
#include "Core/Rendering/Core/AccelerationStructure.h"
#include "Core/Rendering/LayerDefines.h"
#include "Core/Rendering/Vulkan/VkBackendAccess.h"
#include "Core/Rendering/Vulkan/VkPipeline.h"
#include "Core/Rendering/Vulkan/VkTexture.h"
#include <vulkan/vulkan.h>

struct VulkanRayTracingProperties
{
    VkPhysicalDeviceAccelerationStructurePropertiesKHR accelerationStructure{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
};

class Profiler;
struct RendererState;
namespace RenderPasses
{
class PassManager;
}

template <>
class RenderBackendImpl<Vulkan>
{
public:
    virtual ~RenderBackendImpl() = default;
    // Before the window exists (vendor SDK hooks, Vulkan loader for GLFW)
    static void PreWindowSystemInit();
    static stltype::unique_ptr<Profiler> CreateProfiler();

    // Filled from the picked physical device; published to g_renderer right after device creation
    RenderCapabilities QueryCapabilities() const;

    bool Init(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title);

    bool Cleanup();

    bool RecreateSwapChain();

    void InitImGui(const DescriptorPool& pool);
    void ImGuiNewFrame();
    void ShutdownImGui();

    // Vendor upscalers (DLSS / XeSS); Vulkan-only today
    bool IsDLSSSupported() const;
    bool IsDLSSRRSupported() const;
    bool IsXeSSSupported() const;
    bool IsDLSSDebugUIAvailable() const;
    void AddVendorUpscalerPasses(RenderPasses::PassManager& passManager);
    void BeginFrame(u32 frameIdx);
    void DrawVendorSettingsUI();
    void DrawVendorDiagnosticsUI(const RendererState& state);

    bool AreValidationLayersAvailable(const stltype::vector<VkLayerProperties>& availableExtensions);

    void CreateDebugMessenger();

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
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

    VkInstance GetInstance() const
    {
        return m_instance;
    }
    VkDevice GetLogicalDevice() const
    {
        return m_logicalDevice;
    }
    VkPhysicalDevice GetPhysicalDevice() const
    {
        return m_physicalDevice;
    }
    VulkanQueues GetQueues() const
    {
        return {m_graphicsQueue, m_presentQueue, m_transferQueue, m_computeQueue};
    }
    VkSwapchainKHR GetSwapchain() const
    {
        return m_swapChain;
    }
    const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const
    {
        return m_memoryProperties;
    }

    VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(VkInstance instance,
                                                                  const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                                                  const VkAllocationCallbacks* pAllocator,
                                                                  VkDebugUtilsMessengerEXT* pMessenger)
    {
        auto func =
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr)
        {
            func(instance, pCreateInfo, pAllocator, pMessenger);
        }
        return VK_SUCCESS;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(VkInstance instance,
                                                                   const VkAllocationCallbacks* pAllocator,
                                                                   VkDebugUtilsMessengerEXT* pMessenger)
    {
        auto func =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr)
        {
            func(instance, *pMessenger, pAllocator);
        }
        return VK_SUCCESS;
    }

private:
    bool CreateInstance(uint32_t screenWidth, uint32_t screenHeight, stltype::string_view title);
    bool CreateLogicalDevice();
    bool AcquireDeviceQueues();
    bool PickPhysicalDevice();
    bool IsDeviceSuitable(VkPhysicalDevice device);
    bool AreExtensionsSupported(VkPhysicalDevice device);
    bool DeviceSupportsDLSSRequirements(VkPhysicalDevice device) const;
    // MoltenVK has no ray tracing, so it is optional on macOS and required elsewhere
    static bool IsRayTracingRequired()
    {
        return !Engine::IsMacOS();
    }
    bool QueryRayTracingSupport(VkPhysicalDevice device,
                                VulkanRayTracingProperties* pProperties = nullptr) const;
    void PublishDLSSSupport(bool supported) const;
    void PublishRTSupport(bool supported) const;

    void CreateAndDistributeDepthBuffer();
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const stltype::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR ChooseSwapPresentMode(const stltype::vector<VkPresentModeKHR>& availablePresentModes);
    DirectX::XMUINT2 GetWindowFramebufferExtent() const;
    DirectX::XMUINT2 ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    bool CreateSwapChain(VkSwapchainKHR oldSwapchain);
    void CreateSwapChainImages();

    /// @brief Creates the surface with glfw to interface with the window.
    /// @return Whether the surface was created.
    bool CreateSurface();

    bool CreateGraphicsPipeline();

    VkDebugUtilsMessengerEXT m_debugMessenger;
    VkInstance m_instance{VK_NULL_HANDLE};
    VkDevice m_logicalDevice{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkQueue m_graphicsQueue{VK_NULL_HANDLE};
    VkSurfaceKHR m_surface{VK_NULL_HANDLE}; ///< Surface that establishes
                                            ///< connection to the glfw window.
    VkQueue m_presentQueue{VK_NULL_HANDLE};
    VkPhysicalDeviceMemoryProperties m_memoryProperties{};
    VkQueue m_transferQueue{VK_NULL_HANDLE};
    VkQueue m_computeQueue{VK_NULL_HANDLE};
    QueueFamilyIndices m_indices;
    VkSwapchainKHR m_swapChain{VK_NULL_HANDLE};
    stltype::vector<VkImage> m_swapChainImages;
    mathstl::Vector2 m_swapChainExtent;
    bool m_dlssSupportAvailable{false};
    // MoltenVK has no ray tracing; RT is optional there and required elsewhere
    bool m_rayTracingSupported{false};
    bool m_hasPortabilitySubset{false};
    VulkanRayTracingProperties m_rayTracingProperties{};
};
