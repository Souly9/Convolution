#include "StreamlineManager.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "Core/Global/GlobalVariables.h"
#include <vulkan/vulkan.h>
#include <sl_helpers_vk.h>
#include <windows.h>
#include <iostream>

namespace Nvidia
{
bool StreamlineManager::s_initialized = false;
static bool g_dlssConfigured = false;
static u32 g_dlssWidth = 0;
static u32 g_dlssHeight = 0;
static sl::DLSSMode g_dlssMode = sl::DLSSMode::eOff;
static bool g_dlssLastConfigureFailed = false;
static u32 g_dlssFailedWidth = 0;
static u32 g_dlssFailedHeight = 0;
static sl::DLSSMode g_dlssFailedMode = sl::DLSSMode::eOff;
static bool g_dlssEvaluateBlocked = false;
static bool g_dlssNeedsReset = false;
static HMODULE g_slModule = nullptr;

// Function pointers for Streamline API
static PFun_slInit* g_slInit = nullptr;
static PFun_slShutdown* g_slShutdown = nullptr;
static PFun_slGetFeatureFunction* g_slGetFeatureFunction = nullptr;
static PFun_slGetNewFrameToken* g_slGetNewFrameToken = nullptr;
static PFun_slSetVulkanInfo* g_slSetVulkanInfo = nullptr;
static PFun_slSetTagForFrame* g_slSetTagForFrame = nullptr;
static PFun_slSetConstants* g_slSetConstants = nullptr;
static PFun_slEvaluateFeature* g_slEvaluateFeature = nullptr;
static PFun_slIsFeatureSupported* g_slIsFeatureSupported = nullptr;
static PFun_slGetFeatureRequirements* g_slGetFeatureRequirements = nullptr;
static PFun_slAllocateResources* g_slAllocateResources = nullptr;
static PFun_slFreeResources* g_slFreeResources = nullptr;
static PFN_vkGetDeviceProcAddr g_slVkGetDeviceProcAddr = nullptr;
static u32 g_slGraphicsQueueStartIndex = 0;
static u32 g_slComputeQueueStartIndex = 0;

template <typename T>
static T ResolveDeviceHook(VkDevice device, const char* name, T& pCachedFn)
{
    if (pCachedFn)
        return pCachedFn;

    if (g_slVkGetDeviceProcAddr)
        pCachedFn = reinterpret_cast<T>(g_slVkGetDeviceProcAddr(device, name));

    if (!pCachedFn)
        pCachedFn = reinterpret_cast<T>(vkGetDeviceProcAddr(device, name));

    return pCachedFn;
}

static void SL_LoggingCallback(sl::LogType type, const char* msg)
{
    std::string prefix = "[Streamline] ";
    if (type == sl::LogType::eError) prefix += "[ERROR] ";
    else if (type == sl::LogType::eWarn) prefix += "[WARN] ";
    else prefix += "[INFO] ";
    
    std::cout << prefix << msg << std::endl;
}

bool StreamlineManager::EarlyInit()
{
    // Try loading from local directory first
    g_slModule = LoadLibraryA("sl.interposer.dll");
    if (!g_slModule)
    {
        g_slModule = LoadLibraryA("External/additional_libs/NVStreamline/sl.interposer.dll");
    }

    if (!g_slModule)
    {
        std::cout << "[StreamlineManager] Failed to load sl.interposer.dll" << std::endl;
        return false;
    }

    g_slInit = (PFun_slInit*)GetProcAddress(g_slModule, "slInit");
    g_slShutdown = (PFun_slShutdown*)GetProcAddress(g_slModule, "slShutdown");
    g_slGetFeatureFunction = (PFun_slGetFeatureFunction*)GetProcAddress(g_slModule, "slGetFeatureFunction");
    g_slGetNewFrameToken = (PFun_slGetNewFrameToken*)GetProcAddress(g_slModule, "slGetNewFrameToken");
    g_slSetVulkanInfo = (PFun_slSetVulkanInfo*)GetProcAddress(g_slModule, "slSetVulkanInfo");
    g_slSetTagForFrame = (PFun_slSetTagForFrame*)GetProcAddress(g_slModule, "slSetTagForFrame");
    g_slSetConstants = (PFun_slSetConstants*)GetProcAddress(g_slModule, "slSetConstants");
    g_slEvaluateFeature = (PFun_slEvaluateFeature*)GetProcAddress(g_slModule, "slEvaluateFeature");
    g_slIsFeatureSupported = (PFun_slIsFeatureSupported*)GetProcAddress(g_slModule, "slIsFeatureSupported");
    g_slGetFeatureRequirements = (PFun_slGetFeatureRequirements*)GetProcAddress(g_slModule, "slGetFeatureRequirements");
    g_slAllocateResources = (PFun_slAllocateResources*)GetProcAddress(g_slModule, "slAllocateResources");
    g_slFreeResources = (PFun_slFreeResources*)GetProcAddress(g_slModule, "slFreeResources");
    g_slVkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)GetProcAddress(g_slModule, "vkGetDeviceProcAddr");

    if (!g_slInit || !g_slShutdown || !g_slGetFeatureFunction || !g_slGetNewFrameToken || 
        !g_slSetVulkanInfo || !g_slSetTagForFrame || !g_slSetConstants || !g_slEvaluateFeature ||
        !g_slIsFeatureSupported || !g_slGetFeatureRequirements || !g_slAllocateResources || !g_slFreeResources || !g_slVkGetDeviceProcAddr)
    {
        std::cout << "[StreamlineManager] Failed to resolve function pointers" << std::endl;
        return false;
    }

    // Call slInit EARLY so interposer can hook Vulkan creation
    sl::Preferences pref{};
    static sl::Feature features[] = {sl::kFeatureDLSS};
    pref.featuresToLoad = features;
    pref.numFeaturesToLoad = 1;
    pref.renderAPI = sl::RenderAPI::eVulkan;
    pref.flags |= sl::PreferenceFlags::eUseFrameBasedResourceTagging;
    pref.flags |= sl::PreferenceFlags::eUseManualHooking;
    pref.logMessageCallback = SL_LoggingCallback;
    pref.logLevel = sl::LogLevel::eVerbose;
    
    // Set development IDs
    pref.applicationId = 231313132; 
    pref.engine = sl::EngineType::eCustom;
    pref.engineVersion = "1.0.0";
    pref.projectId = "a0f57b54-1daf-4934-90ae-c4035c19df04";

    sl::Result res = g_slInit(pref, sl::kSDKVersion);
    if (res != sl::Result::eOk) 
    {
        std::cout << "[StreamlineManager] slInit failed with error: " << (int)res << std::endl;
        return false;
    }

    return true;
}

bool StreamlineManager::Init()
{
    if (s_initialized)
        return true;
    if (!g_slSetVulkanInfo) return false;

    const auto& context = VkGlobals::GetContext();
    const auto& indices = VkGlobals::GetQueueFamilyIndices();
    
    sl::VulkanInfo info{};
    info.instance = context.Instance;
    info.device = context.Device;
    info.physicalDevice = context.PhysicalDevice;
    info.graphicsQueueFamily = indices.graphicsFamily.value_or(0);
    info.computeQueueFamily = indices.computeFamily.value_or(0);
    info.graphicsQueueIndex = g_slGraphicsQueueStartIndex;
    info.computeQueueIndex = g_slComputeQueueStartIndex;

    sl::Result res = g_slSetVulkanInfo(info);
    if (res != sl::Result::eOk) 
    {
        DEBUG_ASSERT(false);
    }

    // Check support
    sl::AdapterInfo adapterInfo{};
    adapterInfo.vkPhysicalDevice = context.PhysicalDevice;
    res = g_slIsFeatureSupported(sl::kFeatureDLSS, adapterInfo);
    if (res != sl::Result::eOk)
    {
        DEBUG_ASSERT(false);

    }

    sl::FeatureRequirements dlssRequirements{};
    if (GetDLSSFeatureRequirements(dlssRequirements))
    {
        std::cout << "[StreamlineManager] DLSS requirements: gfxQueues=" << dlssRequirements.vkNumGraphicsQueuesRequired
                  << ", computeQueues=" << dlssRequirements.vkNumComputeQueuesRequired
                  << ", instanceExts=" << dlssRequirements.vkNumInstanceExtensions
                  << ", deviceExts=" << dlssRequirements.vkNumDeviceExtensions << std::endl;
    }

    s_initialized = true;
    return true;
}

void StreamlineManager::Shutdown()
{
    if (s_initialized && g_slShutdown)
    {
        g_slShutdown();
        s_initialized = false;
        g_dlssConfigured = false;
        g_dlssWidth = 0;
        g_dlssHeight = 0;
        g_dlssMode = sl::DLSSMode::eOff;
        g_dlssLastConfigureFailed = false;
        g_dlssFailedWidth = 0;
        g_dlssFailedHeight = 0;
        g_dlssFailedMode = sl::DLSSMode::eOff;
        g_dlssEvaluateBlocked = false;
        g_dlssNeedsReset = false;
    }
}

bool StreamlineManager::GetFrameToken(u32 frameIdx, sl::FrameToken*& pFrameToken)
{
    (void)frameIdx;
    if (!s_initialized || !g_slGetNewFrameToken) return false;
    return g_slGetNewFrameToken(pFrameToken, nullptr) == sl::Result::eOk && pFrameToken != nullptr;
}

bool StreamlineManager::GetDLSSFeatureRequirements(sl::FeatureRequirements& requirements)
{
    if (!g_slGetFeatureRequirements)
        return false;
    return g_slGetFeatureRequirements(sl::kFeatureDLSS, requirements) == sl::Result::eOk;
}

bool StreamlineManager::GetDLSSOptimalSettings(u32 width, u32 height, sl::DLSSMode mode, sl::DLSSOptimalSettings& settings)
{
    if (!s_initialized) return false;
    sl::DLSSOptions options{};
    options.mode = mode;
    options.outputWidth = width;
    options.outputHeight = height;
    return slDLSSGetOptimalSettings(options, settings) == sl::Result::eOk;
}

void StreamlineManager::SetVulkanQueueStartIndices(u32 graphicsQueueIndex, u32 computeQueueIndex)
{
    g_slGraphicsQueueStartIndex = graphicsQueueIndex;
    g_slComputeQueueStartIndex = computeQueueIndex;
}

bool StreamlineManager::SetDLSSOptions(u32 width, u32 height, sl::DLSSMode mode)
{
    if (!s_initialized || !g_slGetFeatureFunction) return false;

    sl::DLSSOptions options{};
    options.mode = mode;
    options.outputWidth = width;
    options.outputHeight = height;
    options.colorBuffersHDR = sl::Boolean::eTrue;

    sl::ViewportHandle viewport(0);
    const auto res = slDLSSSetOptions(viewport, options);
    if (res != sl::Result::eOk)
    {
        std::cout << "[StreamlineManager] SetDLSSOptions failed with result: 0x" << std::hex
                  << static_cast<u32>(res) << std::dec << std::endl;
    }
    return res == sl::Result::eOk;
}

bool StreamlineManager::EnsureDLSSConfigured(u32 width, u32 height, sl::DLSSMode mode)
{
    if (!s_initialized || width == 0 || height == 0)
        return false;

    const bool configChanged = !g_dlssConfigured || g_dlssWidth != width || g_dlssHeight != height || g_dlssMode != mode;
    const bool sameAsLastFailedConfig = g_dlssLastConfigureFailed &&
                                         g_dlssFailedWidth == width &&
                                         g_dlssFailedHeight == height &&
                                         g_dlssFailedMode == mode;
    if (!configChanged)
        return true;
    if (sameAsLastFailedConfig)
        return false;

    if (g_dlssConfigured && !FreeResources(sl::kFeatureDLSS))
    {
        std::cout << "[StreamlineManager] Failed to free old DLSS resources during reconfigure" << std::endl;
    }
    g_dlssConfigured = false;

    if (!SetDLSSOptions(width, height, mode))
    {
        std::cout << "[StreamlineManager] slDLSSSetOptions failed (" << width << "x" << height << ")" << std::endl;
        g_dlssLastConfigureFailed = true;
        g_dlssFailedWidth = width;
        g_dlssFailedHeight = height;
        g_dlssFailedMode = mode;
        return false;
    }

    g_dlssConfigured = true;
    g_dlssWidth = width;
    g_dlssHeight = height;
    g_dlssMode = mode;
    g_dlssLastConfigureFailed = false;
    g_dlssFailedWidth = 0;
    g_dlssFailedHeight = 0;
    g_dlssFailedMode = sl::DLSSMode::eOff;
    g_dlssEvaluateBlocked = false;
    g_dlssNeedsReset = true;
    return true;
}

bool StreamlineManager::ConsumeDLSSResetFlag()
{
    const bool needsReset = g_dlssNeedsReset;
    g_dlssNeedsReset = false;
    return needsReset;
}

bool StreamlineManager::FreeResources(sl::Feature feature)
{
    if (!s_initialized || !g_slFreeResources) return false;
    sl::ViewportHandle viewport(0);
    const auto res = g_slFreeResources(feature, viewport);
    return res == sl::Result::eOk;
}

bool StreamlineManager::EvaluateDLSS(VkCommandBuffer cmdBuf, const sl::FrameToken& frameToken)
{
    if (!s_initialized || !g_slEvaluateFeature)
        return false;
    if (g_dlssEvaluateBlocked)
        return false;

    sl::ViewportHandle viewport(0);
    const sl::BaseStructure* inputs[] = {&viewport};
    const auto res = g_slEvaluateFeature(sl::kFeatureDLSS, frameToken, inputs, 1, (sl::CommandBuffer*)cmdBuf);
    if (res != sl::Result::eOk)
    {
        std::cout << "[StreamlineManager] EvaluateDLSS failed with result: 0x" << std::hex
                  << static_cast<u32>(res) << std::dec << std::endl;
        if (res == sl::Result::eErrorNGXFailed)
        {
            g_dlssEvaluateBlocked = true;
            std::cout << "[StreamlineManager] Blocking further DLSS evaluate calls for current config to prevent NGX recreation/VRAM growth" << std::endl;
        }
    }
    return res == sl::Result::eOk;
}

bool StreamlineManager::IsDLSSEvaluateBlocked()
{
    return g_dlssEvaluateBlocked;
}

bool StreamlineManager::AllocateResources(VkCommandBuffer cmdBuf, sl::Feature feature)
{
    if (!s_initialized || !g_slAllocateResources)
        return false;
    sl::ViewportHandle viewport(0);
    const auto res = g_slAllocateResources((sl::CommandBuffer*)cmdBuf, feature, viewport);
    if (res != sl::Result::eOk)
    {
        std::cout << "[StreamlineManager] AllocateResources failed with result: 0x" << std::hex
                  << static_cast<u32>(res) << std::dec << std::endl;
    }
    return res == sl::Result::eOk;
}

} // namespace Nvidia

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice device,
                                                               const VkSwapchainCreateInfoKHR* pCreateInfo,
                                                               const VkAllocationCallbacks* pAllocator,
                                                               VkSwapchainKHR* pSwapchain)
{
    static PFN_vkCreateSwapchainKHR s_pHook = nullptr;
    const auto pHook = Nvidia::ResolveDeviceHook(device, "vkCreateSwapchainKHR", s_pHook);
    return pHook ? pHook(device, pCreateInfo, pAllocator, pSwapchain) : VK_ERROR_INITIALIZATION_FAILED;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroySwapchainKHR(VkDevice device,
                                                            VkSwapchainKHR swapchain,
                                                            const VkAllocationCallbacks* pAllocator)
{
    static PFN_vkDestroySwapchainKHR s_pHook = nullptr;
    const auto pHook = Nvidia::ResolveDeviceHook(device, "vkDestroySwapchainKHR", s_pHook);
    if (pHook)
        pHook(device, swapchain, pAllocator);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetSwapchainImagesKHR(VkDevice device,
                                                                  VkSwapchainKHR swapchain,
                                                                  uint32_t* pSwapchainImageCount,
                                                                  VkImage* pSwapchainImages)
{
    static PFN_vkGetSwapchainImagesKHR s_pHook = nullptr;
    const auto pHook = Nvidia::ResolveDeviceHook(device, "vkGetSwapchainImagesKHR", s_pHook);
    return pHook ? pHook(device, swapchain, pSwapchainImageCount, pSwapchainImages) : VK_ERROR_INITIALIZATION_FAILED;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImageKHR(VkDevice device,
                                                                VkSwapchainKHR swapchain,
                                                                uint64_t timeout,
                                                                VkSemaphore semaphore,
                                                                VkFence fence,
                                                                uint32_t* pImageIndex)
{
    static PFN_vkAcquireNextImageKHR s_pHook = nullptr;
    const auto pHook = Nvidia::ResolveDeviceHook(device, "vkAcquireNextImageKHR", s_pHook);
    return pHook ? pHook(device, swapchain, timeout, semaphore, fence, pImageIndex) : VK_ERROR_INITIALIZATION_FAILED;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
{
    static PFN_vkQueuePresentKHR s_pHook = nullptr;
    const auto pHook = Nvidia::ResolveDeviceHook(VkGlobals::GetLogicalDevice(), "vkQueuePresentKHR", s_pHook);
    return pHook ? pHook(queue, pPresentInfo) : VK_ERROR_INITIALIZATION_FAILED;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice device)
{
    static PFN_vkDeviceWaitIdle s_pHook = nullptr;
    const auto pHook = Nvidia::ResolveDeviceHook(device, "vkDeviceWaitIdle", s_pHook);
    return pHook ? pHook(device) : VK_ERROR_INITIALIZATION_FAILED;
}

// Linker redirection
extern "C" {
    sl::Result slInit(const sl::Preferences& pref, uint64_t sdkVersion) {
        return Nvidia::g_slInit ? Nvidia::g_slInit(pref, sdkVersion) : sl::Result::eErrorNotInitialized;
    }
    sl::Result slShutdown() {
        return Nvidia::g_slShutdown ? Nvidia::g_slShutdown() : sl::Result::eErrorNotInitialized;
    }
    sl::Result slGetFeatureFunction(sl::Feature feature, const char* functionName, void*& function) {
        return Nvidia::g_slGetFeatureFunction ? Nvidia::g_slGetFeatureFunction(feature, functionName, function) : sl::Result::eErrorNotInitialized;
    }
    sl::Result slGetNewFrameToken(sl::FrameToken*& token, const uint32_t* frameIndex) {
        return Nvidia::g_slGetNewFrameToken ? Nvidia::g_slGetNewFrameToken(token, frameIndex) : sl::Result::eErrorNotInitialized;
    }
    sl::Result slSetVulkanInfo(const sl::VulkanInfo& info) {
        return Nvidia::g_slSetVulkanInfo ? Nvidia::g_slSetVulkanInfo(info) : sl::Result::eErrorNotInitialized;
    }
    sl::Result slSetTagForFrame(const sl::FrameToken& frame, const sl::ViewportHandle& viewport, const sl::ResourceTag* tags, uint32_t numTags, sl::CommandBuffer* cmdBuffer) {
        return Nvidia::g_slSetTagForFrame ? Nvidia::g_slSetTagForFrame(frame, viewport, tags, numTags, cmdBuffer) : sl::Result::eErrorNotInitialized;
    }
    sl::Result slSetConstants(const sl::Constants& values, const sl::FrameToken& frame, const sl::ViewportHandle& viewport) {
        return Nvidia::g_slSetConstants ? Nvidia::g_slSetConstants(values, frame, viewport) : sl::Result::eErrorNotInitialized;
    }
    sl::Result slEvaluateFeature(sl::Feature feature, const sl::FrameToken& frame, const sl::BaseStructure** inputs, uint32_t numInputs, sl::CommandBuffer* cmdBuffer) {
        return Nvidia::g_slEvaluateFeature ? Nvidia::g_slEvaluateFeature(feature, frame, inputs, numInputs, cmdBuffer) : sl::Result::eErrorNotInitialized;
    }
    sl::Result slIsFeatureSupported(sl::Feature feature, const sl::AdapterInfo& adapterInfo) {
        return Nvidia::g_slIsFeatureSupported ? Nvidia::g_slIsFeatureSupported(feature, adapterInfo) : sl::Result::eErrorNotInitialized;
    }
    sl::Result slAllocateResources(sl::CommandBuffer* cmdBuffer, sl::Feature feature, const sl::ViewportHandle& viewport) {
        return Nvidia::g_slAllocateResources ? Nvidia::g_slAllocateResources(cmdBuffer, feature, viewport) : sl::Result::eErrorNotInitialized;
    }
    sl::Result slFreeResources(sl::Feature feature, const sl::ViewportHandle& viewport) {
        return Nvidia::g_slFreeResources ? Nvidia::g_slFreeResources(feature, viewport) : sl::Result::eErrorNotInitialized;
    }
}
