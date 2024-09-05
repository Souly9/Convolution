#pragma once
#include <vulkan/vulkan_core.h>
#include "VkEnumHelpers.h"

static inline const stltype::vector<const char*> g_validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

static inline const stltype::vector<const char*> g_deviceExtensions = {
    "VK_KHR_swapchain"
}; 

static inline const stltype::vector<VkDynamicState> g_dynamicStates = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
};


#define SWAPCHAINFORMAT VK_FORMAT_B8G8R8A8_SRGB
#define SWAPCHAINCOLORSPACE VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
#define SWAPCHAINPRESENTMODE VK_PRESENT_MODE_MAILBOX_KHR

#ifdef NDEBUG
static inline constexpr bool ENABLE_VALIDATION_LAYERS = false;
#else
static inline constexpr bool ENABLE_VALIDATION_LAYERS = true;
#endif

static inline VkBool32 BoolToVkBool(const bool& val)
{
    return val ? VK_TRUE : VK_FALSE;
}

#define VK_LOGICAL_DEVICE VkGlobals::GetLogicalDevice()