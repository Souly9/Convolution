#pragma once
#include "Core/WindowManager.h"
#include <vulkan/vulkan_core.h>

static inline constexpr u64 MAX_TEXTURES = 4096;

#define VK_LOGICAL_DEVICE VkGlobals::GetLogicalDevice()
#define VK_PHYS_DEVICE    VkGlobals::GetPhysicalDevice()
#define VK_FREE_IF(res, freeFunc)                                                                                      \
    if (res != VK_NULL_HANDLE)                                                                                         \
    {                                                                                                                  \
        freeFunc;                                                                                                      \
        res = VK_NULL_HANDLE;                                                                                          \
    }
#define VK_API_LEVEL_MIN    VK_MAKE_API_VERSION(0, 1, 4, 3)

static inline constexpr VkClearValue g_BlackCLearColor = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
static inline constexpr VkClearValue g_WhiteCLearColor = {{{1.0f, 1.0f, 1.0f, 1.0f}}};

struct RequiredDeviceFeatures
{
    bool needsAnisotropicFiltering{true};
};
static inline const RequiredDeviceFeatures g_requiredDeviceFeatures{};

static inline const stltype::vector<const char*> g_validationLayers = {
#ifdef CONV_DEBUG
    "VK_LAYER_KHRONOS_validation"
#endif
};

static inline const stltype::vector<const char*> g_instanceExtensions = {
    "VK_EXT_swapchain_colorspace", // hdr rendering,
    "VK_EXT_debug_utils"};

static inline const stltype::vector<const char*> g_deviceExtensions = {"VK_KHR_swapchain",
                                                                       "VK_EXT_memory_budget",
                                                                       "VK_NV_device_diagnostic_checkpoints",
                                                                       "VK_NV_device_diagnostics_config",
                                                                       "VK_EXT_device_address_binding_report",
                                                                       "VK_EXT_device_fault",
                                                                       "VK_EXT_scalar_block_layout"};

static inline const stltype::vector<VkDynamicState> g_dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                                       VK_DYNAMIC_STATE_SCISSOR};

#define SWAPCHAINCOLORSPACE  VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT
#define SWAPCHAINPRESENTMODE VK_PRESENT_MODE_FIFO_KHR

#ifdef NDEBUG
static inline constexpr bool ENABLE_VALIDATION_LAYERS = false;
#else
static inline constexpr bool ENABLE_VALIDATION_LAYERS = true;
#endif

static inline VkBool32 Conv(const bool& val)
{
    return val ? VK_TRUE : VK_FALSE;
}

static inline DirectX::XMUINT2 Conv(const VkExtent2D& extent)
{
    return DirectX::XMUINT2(extent.width, extent.height);
}
static inline VkExtent2D Conv(const DirectX::XMUINT2& extent)
{
    return VkExtent2D(extent.x, extent.y);
}
static inline VkOffset2D Conv(const DirectX::XMINT2& extent)
{
    return VkOffset2D(extent.x, extent.y);
}
static inline VkOffset3D Conv(const DirectX::XMINT3& extent)
{
    return VkOffset3D(extent.x, extent.y, extent.z);
}
static inline VkExtent3D Conv(const DirectX::XMUINT3& extent)
{
    return VkExtent3D(extent.x, extent.y, extent.z);
}

// Vulkan function pointers, global
static inline PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectName = VK_NULL_HANDLE;
static inline PFN_vkCmdSetCheckpointNV vkCmdSetCheckpoint = VK_NULL_HANDLE;
static inline PFN_vkCmdBeginDebugUtilsLabelEXT vkBeginDebugUtilsLabel = VK_NULL_HANDLE;
static inline PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabel = VK_NULL_HANDLE;

#include "Core/Rendering/Core/RenderingTypeDefs.h"