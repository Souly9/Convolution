#pragma once
#include "Core/Global/GlobalDefines.h"
#include <vulkan/vulkan.h>
#include <xess/xess_vk.h>

namespace VulkanXeSS
{
class XeSSManager
{
public:
    static bool Initialize();
    static void Shutdown();

    static bool IsSupported();

    static bool EnsureConfigured(VkInstance instance,
                                 VkPhysicalDevice physicalDevice,
                                 VkDevice device,
                                 u32 outputWidth,
                                 u32 outputHeight,
                                 xess_quality_settings_t qualitySetting);

    static xess_context_handle_t GetContext();
    static bool GetOptimalResolution(u32 outputWidth,
                                     u32 outputHeight,
                                     xess_quality_settings_t qualitySetting,
                                     u32& outRenderWidth,
                                     u32& outRenderHeight);

    static bool Execute(VkCommandBuffer cmdBuf, const xess_vk_execute_params_t& execParams);

    static void ResetHistory();
    static bool ConsumeResetFlag();

private:
    static inline bool s_xessDllLoaded{false};
    static inline bool s_initialized{false};
    static inline xess_context_handle_t s_context{nullptr};
    static inline u32 s_currentWidth{0};
    static inline u32 s_currentHeight{0};
    static inline xess_quality_settings_t s_currentQuality{static_cast<xess_quality_settings_t>(0)};
    static inline bool s_needsReset{false};
};
} // namespace VulkanXeSS
