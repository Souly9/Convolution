#pragma once
#include "Core/Global/GlobalDefines.h"
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>
#include "sl.h"
#include "sl_dlss.h"
#include "sl_dlss_d.h"

namespace Nvidia
{
class StreamlineManager
{
public:
    struct DLSSDebugState
    {
        bool streamlineInitialized{false};
        bool featureSupported{false};
        bool imguiPluginAvailable{false};
        bool configured{false};
        bool evaluateBlocked{false};
        bool lastConfigureFailed{false};
        bool needsReset{false};
        bool cameraMotionIncluded{false};
        bool motionVectorsJittered{false};
        bool depthInverted{false};
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
        mathstl::Vector2 motionVectorScale{1.0f, 1.0f};
        f32 optimalSharpness{0.0f};
        f32 nearPlane{0.0f};
        f32 farPlane{0.0f};
        f32 fovRadians{0.0f};
        f32 aspectRatio{1.0f};
        u64 estimatedVRAMUsageInBytes{0};
    };

    static bool EarlyInit();
    static bool Init();
    static void Shutdown();

    static void AcquireNewFrameToken(u32 frameIdx);
    static bool GetFrameToken(u32 frameIdx, sl::FrameToken*& pFrameToken);
    static bool GetDLSSFeatureRequirements(sl::FeatureRequirements& requirements);
    static bool GetDLSSRRFeatureRequirements(sl::FeatureRequirements& requirements);
    static bool GetDLSSOptimalSettings(u32 width, u32 height, sl::DLSSMode mode, sl::DLSSOptimalSettings& settings);
    static bool GetDLSSState(sl::DLSSState& state);

    static void SetVulkanQueueStartIndices(u32 graphicsQueueIndex, u32 computeQueueIndex);
    static void GetVulkanDeviceQueue(VkDevice device, u32 queueFamilyIndex, u32 queueIndex, VkQueue* pQueue);
    static bool EnsureDLSSConfigured(u32 width, u32 height, sl::DLSSMode mode);
    static bool ConsumeDLSSResetFlag();
    static bool SetDLSSOptions(u32 width, u32 height, sl::DLSSMode mode);
    static bool FreeResources(sl::Feature feature);
    static sl::Result SetTagForFrame(const sl::FrameToken& frame,
                                     const sl::ViewportHandle& viewport,
                                     const sl::ResourceTag* tags,
                                     uint32_t numTags,
                                     sl::CommandBuffer* cmdBuffer);
    static sl::Result SetConstants(const sl::Constants& values,
                                   const sl::FrameToken& frame,
                                   const sl::ViewportHandle& viewport);
    static bool EvaluateDLSS(VkCommandBuffer cmdBuf, const sl::FrameToken& frameToken);
    static bool AllocateResources(VkCommandBuffer cmdBuf, sl::Feature feature);
    static bool IsDLSSEvaluateBlocked();
    static DLSSDebugState GetDLSSDebugState();
    static void SetDLSSDebugState(const DLSSDebugState& state);

    static bool EnsureDLSSDConfigured(u32 width, u32 height, sl::DLSSMode mode, const mathstl::Matrix& worldToView, const mathstl::Matrix& viewToWorld);
    static bool ConsumeDLSSDResetFlag();
    static bool SetDLSSDOptions(u32 width, u32 height, sl::DLSSMode mode, const mathstl::Matrix& worldToView, const mathstl::Matrix& viewToWorld);
    static bool EvaluateDLSSD(VkCommandBuffer cmdBuf, const sl::FrameToken& frameToken);
    static bool IsDLSSDConfigured();
    static bool IsDLSSDLastConfigureFailed();
    static bool IsDLSSDEvaluateBlocked();

    static bool IsAvailable() { return s_initialized; }
    static bool IsEarlyInitialized();
    static bool IsDLSSSupported();
    static bool IsDLSSRRSupported();
    static bool IsDLSSDebugUIAvailable();

    static void SetUseRayReconstructionThisFrame(bool use) { s_useRayReconstructionThisFrame = use; }
    static bool GetUseRayReconstructionThisFrame() { return s_useRayReconstructionThisFrame; }

private:
    static bool s_initialized;
    inline static bool s_useRayReconstructionThisFrame{false};
};
} // namespace Nvidia
