#pragma once
#include <vulkan/vulkan_core.h>
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/WindowManager.h"
#include "Core/Memory/MemoryManager.h"

static inline constexpr u64 MAX_TEXTURES = 512;

#define VK_LOGICAL_DEVICE VkGlobals::GetLogicalDevice()
#define VK_PHYS_DEVICE VkGlobals::GetPhysicalDevice()
#define VK_FREE_IF(res, freeFunc) if(res != VK_NULL_HANDLE) { freeFunc; res = VK_NULL_HANDLE; }
#define DEPTH_BUFFER_FORMAT VK_FORMAT_D32_SFLOAT

static inline constexpr VkClearValue g_BlackCLearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
static inline constexpr VkClearValue g_WhiteCLearColor = { {{1.0f, 1.0f, 1.0f, 1.0f}} };

struct RequiredDeviceFeatures
{
    bool needsAnisotropicFiltering{ true };
};
static inline const RequiredDeviceFeatures g_requiredDeviceFeatures{};

static inline const stltype::vector<const char*> g_validationLayers = {
#ifdef CONV_DEBUG
    "VK_LAYER_KHRONOS_validation"
#endif
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