#pragma once
#include "Core/Global/GlobalDefines.h"

#ifndef USE_VULKAN
// Streamline/DLSS is Vulkan-only; this stub keeps shared code compiling on other backends
namespace Nvidia
{
class StreamlineManager
{
public:
    static bool EarlyInit() { return false; }
    static bool Init() { return false; }
    static void Shutdown() {}
    static void AcquireNewFrameToken(u32 frameIdx) {}
    static bool IsAvailable() { return false; }
    static bool IsEarlyInitialized() { return false; }
    static bool IsDLSSSupported() { return false; }
    static bool IsDLSSRRSupported() { return false; }
    static bool IsDLSSDebugUIAvailable() { return false; }
};
} // namespace Nvidia
#else
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>
#include "sl.h"
#include "sl_dlss.h"
#include "sl_dlss_d.h"

namespace Nvidia
{
// SR and RR share configuration and evaluation, only options and tags differ
enum class DLSSVariant : u8
{
    SuperResolution,
    RayReconstruction,
    Count
};

class StreamlineManager
{
public:
    enum class LogVerbosity : u8
    {
        Off,
        Errors,
        Warnings,
        Info,
    };

    // Runtime knobs set from the debug UI
    struct DebugSettings
    {
        // Copy the input instead of evaluating, for A/B comparisons
        bool bypass{false};
        // Tag DLSSExposure with the tonemapper exposure instead of DLSS auto exposure
        bool useExposureTexture{false};
        bool flipJitterX{false};
        bool flipJitterY{false};
        bool overrideMotionVectorScale{false};
        mathstl::Vector2 motionVectorScale{-0.5f, 0.5f};
        // Applied to every SR quality mode
        sl::DLSSPreset preset{sl::DLSSPreset::eDefault};
        bool invertIndicatorAxisX{false};
        bool invertIndicatorAxisY{false};
        LogVerbosity logVerbosity{LogVerbosity::Warnings};
        // Bumped by the UI to force one history reset
        u32 resetGeneration{0};
    };

    struct DLSSDebugState
    {
        bool streamlineInitialized{false};
        bool featureSupported{false};
        bool rrSupported{false};
        bool imguiPluginAvailable{false};
        bool presentRoutedThroughStreamline{false};
        DLSSVariant variant{DLSSVariant::SuperResolution};
        bool configured{false};
        bool evaluateBlocked{false};
        bool lastConfigureFailed{false};
        bool bypassed{false};
        bool reset{false};
        bool exposureTextureTagged{false};
        u64 evaluateCallCount{0};
        sl::DLSSMode configuredMode{sl::DLSSMode::eOff};
        sl::Result lastSetConstantsResult{sl::Result::eOk};
        sl::Result lastTagResult{sl::Result::eOk};
        sl::Result lastEvaluateResult{sl::Result::eOk};
        u32 inputWidth{0};
        u32 inputHeight{0};
        u32 outputWidth{0};
        u32 outputHeight{0};
        u32 optimalRenderWidth{0};
        u32 optimalRenderHeight{0};
        u32 renderWidthMin{0};
        u32 renderHeightMin{0};
        u32 renderWidthMax{0};
        u32 renderHeightMax{0};
        mathstl::Vector2 jitter{};
        mathstl::Vector2 motionVectorScale{};
        f32 nearPlane{0.0f};
        f32 farPlane{0.0f};
        f32 fovRadians{0.0f};
        f32 aspectRatio{1.0f};
        u64 estimatedVRAMUsageInBytes{0};
    };

    struct VersionInfo
    {
        sl::Version sdk{};
        sl::Version dlssPlugin{};
        sl::Version dlssNGX{};
        sl::Version rrPlugin{};
        sl::Version rrNGX{};
        sl::FeatureRequirementFlags dlssFlags{};
        sl::FeatureRequirementFlags rrFlags{};
        bool developmentPlugins{false};
    };

    // Swapchain entry points; they point into sl.interposer when routing is on (CONV_SL_OVERLAY=1)
    struct SwapchainFunctions
    {
        PFN_vkCreateSwapchainKHR createSwapchain{vkCreateSwapchainKHR};
        PFN_vkDestroySwapchainKHR destroySwapchain{vkDestroySwapchainKHR};
        PFN_vkGetSwapchainImagesKHR getSwapchainImages{vkGetSwapchainImagesKHR};
        PFN_vkAcquireNextImageKHR acquireNextImage{vkAcquireNextImageKHR};
        PFN_vkQueuePresentKHR queuePresent{vkQueuePresentKHR};
        PFN_vkDeviceWaitIdle deviceWaitIdle{vkDeviceWaitIdle};
    };

    static bool EarlyInit();
    static bool Init();
    static void Shutdown();

    static void AcquireNewFrameToken(u32 frameIdx);
    static bool GetFrameToken(u32 frameIdx, sl::FrameToken*& pFrameToken);
    static bool GetDLSSFeatureRequirements(sl::FeatureRequirements& requirements);
    static bool GetDLSSRRFeatureRequirements(sl::FeatureRequirements& requirements);

    static void SetVulkanQueueStartIndices(u32 graphicsQueueIndex, u32 computeQueueIndex);
    static void GetVulkanDeviceQueue(VkDevice device, u32 queueFamilyIndex, u32 queueIndex, VkQueue* pQueue);

    // Applies options when size, mode or debug options change (RR also needs the camera every frame)
    static bool EnsureConfigured(DLSSVariant variant,
                                 u32 outputWidth,
                                 u32 outputHeight,
                                 sl::DLSSMode mode,
                                 const mathstl::Matrix& worldToView,
                                 const mathstl::Matrix& viewToWorld);
    static bool ConsumeResetFlag(DLSSVariant variant);
    static bool IsEvaluateBlocked(DLSSVariant variant);
    static sl::Result SetTagForFrame(const sl::FrameToken& frame,
                                     const sl::ViewportHandle& viewport,
                                     const sl::ResourceTag* tags,
                                     uint32_t numTags,
                                     sl::CommandBuffer* cmdBuffer);
    static sl::Result SetConstants(const sl::Constants& values,
                                   const sl::FrameToken& frame,
                                   const sl::ViewportHandle& viewport);
    static bool Evaluate(DLSSVariant variant, VkCommandBuffer cmdBuf, const sl::FrameToken& frameToken);

    static DebugSettings GetDebugSettings();
    static void SetDebugSettings(const DebugSettings& settings);
    static DLSSDebugState GetDLSSDebugState();
    static void SetDLSSDebugState(const DLSSDebugState& state);
    static VersionInfo GetVersionInfo();

    static const SwapchainFunctions& GetSwapchainFunctions();
    static bool IsPresentRoutedThroughStreamline();

    static bool IsAvailable() { return s_initialized; }
    static bool IsEarlyInitialized();
    static bool IsDLSSSupported();
    static bool IsDLSSRRSupported();
    // Streamline's own ImGui overlay (Ctrl+Shift+Home) needs its plugin and the routed present
    static bool IsDLSSDebugUIAvailable();

private:
    static bool s_initialized;
};
} // namespace Nvidia
#endif // USE_VULKAN
