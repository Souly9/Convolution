// Streamline/DLSS and XeSS runtimes only exist on Windows; these stubs report "unsupported" so the
// Vulkan backend links elsewhere (MoltenVK on macOS). The real sources are excluded from the build there.
#if defined(USE_VULKAN) && !defined(_WIN32)
#include "Core/Rendering/Core/Nvidia/StreamlineManager.h"
#include "Core/Rendering/Vulkan/XeSS/XeSSManager.h"

namespace Nvidia
{
bool StreamlineManager::s_initialized = false;

namespace
{
StreamlineManager::SwapchainFunctions g_swapchainFunctions{};
}

bool StreamlineManager::EarlyInit() { return false; }
bool StreamlineManager::Init() { return false; }
void StreamlineManager::Shutdown() {}
void StreamlineManager::AcquireNewFrameToken(u32 frameIdx) {}
bool StreamlineManager::GetFrameToken(u32 frameIdx, sl::FrameToken*& pFrameToken) { return false; }
bool StreamlineManager::GetDLSSFeatureRequirements(sl::FeatureRequirements& requirements) { return false; }
bool StreamlineManager::GetDLSSRRFeatureRequirements(sl::FeatureRequirements& requirements) { return false; }
void StreamlineManager::SetVulkanQueueStartIndices(u32 graphicsQueueIndex, u32 computeQueueIndex) {}
void StreamlineManager::GetVulkanDeviceQueue(VkDevice device, u32 queueFamilyIndex, u32 queueIndex, VkQueue* pQueue)
{
    vkGetDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
}
bool StreamlineManager::EnsureConfigured(DLSSVariant variant,
                                         u32 outputWidth,
                                         u32 outputHeight,
                                         sl::DLSSMode mode,
                                         const mathstl::Matrix& worldToView,
                                         const mathstl::Matrix& viewToWorld)
{
    return false;
}
bool StreamlineManager::ConsumeResetFlag(DLSSVariant variant) { return false; }
bool StreamlineManager::IsEvaluateBlocked(DLSSVariant variant) { return true; }
sl::Result StreamlineManager::SetTagForFrame(const sl::FrameToken& frame,
                                             const sl::ViewportHandle& viewport,
                                             const sl::ResourceTag* tags,
                                             uint32_t numTags,
                                             sl::CommandBuffer* cmdBuffer)
{
    return sl::Result::eErrorFeatureNotSupported;
}
sl::Result StreamlineManager::SetConstants(const sl::Constants& values, const sl::FrameToken& frame, const sl::ViewportHandle& viewport)
{
    return sl::Result::eErrorFeatureNotSupported;
}
bool StreamlineManager::Evaluate(DLSSVariant variant, VkCommandBuffer cmdBuf, const sl::FrameToken& frameToken) { return false; }
StreamlineManager::DebugSettings StreamlineManager::GetDebugSettings() { return {}; }
void StreamlineManager::SetDebugSettings(const DebugSettings& settings) {}
StreamlineManager::DLSSDebugState StreamlineManager::GetDLSSDebugState() { return {}; }
void StreamlineManager::SetDLSSDebugState(const DLSSDebugState& state) {}
StreamlineManager::VersionInfo StreamlineManager::GetVersionInfo() { return {}; }
const StreamlineManager::SwapchainFunctions& StreamlineManager::GetSwapchainFunctions() { return g_swapchainFunctions; }
bool StreamlineManager::IsPresentRoutedThroughStreamline() { return false; }
bool StreamlineManager::IsEarlyInitialized() { return false; }
bool StreamlineManager::IsDLSSSupported() { return false; }
bool StreamlineManager::IsDLSSRRSupported() { return false; }
bool StreamlineManager::IsDLSSDebugUIAvailable() { return false; }
} // namespace Nvidia

namespace VulkanXeSS
{
bool XeSSManager::Initialize() { return false; }
void XeSSManager::Shutdown() {}
bool XeSSManager::IsSupported() { return false; }
bool XeSSManager::EnsureConfigured(VkInstance instance,
                                   VkPhysicalDevice physicalDevice,
                                   VkDevice device,
                                   u32 outputWidth,
                                   u32 outputHeight,
                                   xess_quality_settings_t qualitySetting)
{
    return false;
}
xess_context_handle_t XeSSManager::GetContext() { return nullptr; }
bool XeSSManager::GetOptimalResolution(u32 outputWidth, u32 outputHeight, xess_quality_settings_t qualitySetting, u32& outRenderWidth, u32& outRenderHeight) { return false; }
bool XeSSManager::Execute(VkCommandBuffer cmdBuf, const xess_vk_execute_params_t& execParams) { return false; }
void XeSSManager::ResetHistory() {}
bool XeSSManager::ConsumeResetFlag() { return false; }
} // namespace VulkanXeSS

// Declared extern "C" in xess_vk.h; report "nothing required" so the backend's extension/feature setup is unchanged
xess_result_t xessVKGetRequiredInstanceExtensions(uint32_t* instanceExtensionsCount, const char* const** instanceExtensions, uint32_t* minVkApiVersion)
{
    *instanceExtensionsCount = 0;
    *instanceExtensions = nullptr;
    *minVkApiVersion = 0;
    return XESS_RESULT_SUCCESS;
}
xess_result_t xessVKGetRequiredDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t* deviceExtensionsCount, const char* const** deviceExtensions)
{
    *deviceExtensionsCount = 0;
    *deviceExtensions = nullptr;
    return XESS_RESULT_SUCCESS;
}
xess_result_t xessVKGetRequiredDeviceFeatures(VkInstance instance, VkPhysicalDevice physicalDevice, void** features)
{
    return XESS_RESULT_SUCCESS;
}
#endif
