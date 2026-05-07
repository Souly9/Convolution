#include "StreamlineManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Global/ThreadBase.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include <atomic>
#include <sl_helpers_vk.h>
#include <string>
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>
#include <windows.h>

namespace Nvidia
{
bool StreamlineManager::s_initialized = false;
static bool g_slInitialized = false;
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
static bool g_dlssSupported = false;
static bool g_imguiPluginAvailable = false;
static bool g_imguiSupported = false;
static HMODULE g_slModule = nullptr;
static bool g_slModuleOwned = false;
static ProfiledLockable(CustomMutex, g_dlssDebugStateMutex);
static StreamlineManager::DLSSDebugState g_dlssDebugState{};

static PFN_vkCreateInstance g_slVkCreateInstance = nullptr;
static PFN_vkGetInstanceProcAddr g_slVkGetInstanceProcAddr = nullptr;
static PFN_vkGetDeviceProcAddr g_slVkGetDeviceProcAddr = nullptr;
static u32 g_slGraphicsQueueStartIndex = 0;
static u32 g_slComputeQueueStartIndex = 0;
static std::atomic<u64> g_slAcquireCallCount = 0;
static std::atomic<u64> g_slPresentCallCount = 0;

static bool ShouldLogHookProgress(u64 count)
{
    return count <= 3 || (count % 240ull) == 0;
}

static bool IsRenderDocLoaded()
{
    return GetModuleHandleA("renderdoc.dll") != nullptr;
}

static bool FileExists(const char* path)
{
    const DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static bool DirectoryExists(const wchar_t* path)
{
    const DWORD attrs = GetFileAttributesW(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

static std::wstring GetAbsolutePath(const wchar_t* path)
{
    wchar_t buffer[MAX_PATH]{};
    const DWORD len = GetFullPathNameW(path, MAX_PATH, buffer, nullptr);
    return len > 0 && len < MAX_PATH ? std::wstring(buffer, len) : std::wstring(path);
}

template <typename T>
static bool LoadRequiredProc(const char* name, T& proc)
{
    proc = reinterpret_cast<T>(GetProcAddress(g_slModule, name));
    if (!proc)
        DEBUG_LOG_WARNF("[StreamlineManager] Failed to resolve Streamline export '{}'", name);
    return proc != nullptr;
}

static void ResetStreamlineFunctionPointers()
{
    g_slVkCreateInstance = nullptr;
    g_slVkGetInstanceProcAddr = nullptr;
    g_slVkGetDeviceProcAddr = nullptr;
}

static void ReleaseStreamlineLibrary()
{
    if (g_slModuleOwned && g_slModule)
    {
        FreeLibrary(g_slModule);
    }
    g_slModule = nullptr;
    g_slModuleOwned = false;
    ResetStreamlineFunctionPointers();
}

template <typename T>
static T ResolveStreamlineDeviceProc(VkDevice device, const char* name, T& pCachedFn)
{
    if (pCachedFn)
        return pCachedFn;

    if (g_slVkGetDeviceProcAddr)
        pCachedFn = reinterpret_cast<T>(g_slVkGetDeviceProcAddr(device, name));

    if (!pCachedFn)
        pCachedFn = reinterpret_cast<T>(vkGetDeviceProcAddr(device, name));

    return pCachedFn;
}

static PFN_vkGetInstanceProcAddr GetNativeVkGetInstanceProcAddr()
{
    static PFN_vkGetInstanceProcAddr s_pNativeVkGetInstanceProcAddr = []() -> PFN_vkGetInstanceProcAddr
    {
        HMODULE vulkanModule = GetModuleHandleA("vulkan-1.dll");
        if (!vulkanModule)
            vulkanModule = LoadLibraryA("vulkan-1.dll");
        return vulkanModule
                   ? reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(vulkanModule, "vkGetInstanceProcAddr"))
                   : nullptr;
    }();
    return s_pNativeVkGetInstanceProcAddr;
}

template <typename T>
static T ResolveStreamlineInstanceProc(VkInstance instance, const char* name, T& pCachedFn)
{
    if (pCachedFn)
        return pCachedFn;

    if (g_slVkGetInstanceProcAddr)
        pCachedFn = reinterpret_cast<T>(g_slVkGetInstanceProcAddr(instance, name));

    if (!pCachedFn)
    {
        const auto pNativeProc = GetNativeVkGetInstanceProcAddr();
        if (pNativeProc)
            pCachedFn = reinterpret_cast<T>(pNativeProc(instance, name));
    }

    return pCachedFn;
}

static void SL_LoggingCallback(sl::LogType type, const char* msg)
{
    if (type == sl::LogType::eError)
    {
        DEBUG_LOG_ERRF("[Streamline] [ERROR] {}", msg);
    }
    else if (type == sl::LogType::eWarn)
    {
        DEBUG_LOG_WARNF("[Streamline] [WARN] {}", msg);
    }
    else
    {
        DEBUG_LOGF("[Streamline] [INFO] {}", msg);
    }
}

bool StreamlineManager::EarlyInit()
{
    if (g_slInitialized)
        return true;

    if (IsRenderDocLoaded())
    {
        DEBUG_LOG_WARN("[StreamlineManager] RenderDoc detected. DLSS/Streamline disabled for this run.");
        return false;
    }

    g_slModule = GetModuleHandleA("sl.interposer.dll");
    if (!g_slModule)
    {
        g_slModule = LoadLibraryA("sl.interposer.dll");
        g_slModuleOwned = g_slModule != nullptr;
    }
    if (!g_slModule)
    {
        g_slModule = LoadLibraryA("External/additional_libs/NVStreamline/sl.interposer.dll");
        g_slModuleOwned = g_slModule != nullptr;
    }

    if (!g_slModule)
    {
        DEBUG_LOG_WARN("[StreamlineManager] Failed to load sl.interposer.dll");
        ReleaseStreamlineLibrary();
        return false;
    }

    bool loaded = true;
    loaded &= LoadRequiredProc("vkCreateInstance", g_slVkCreateInstance);
    loaded &= LoadRequiredProc("vkGetInstanceProcAddr", g_slVkGetInstanceProcAddr);
    loaded &= LoadRequiredProc("vkGetDeviceProcAddr", g_slVkGetDeviceProcAddr);

    if (!loaded)
    {
        ReleaseStreamlineLibrary();
        return false;
    }

    // Call slInit EARLY so interposer can hook Vulkan creation
    sl::Preferences pref{};
    g_imguiPluginAvailable = false;
    sl::Feature features[] = {sl::kFeatureDLSS};
    pref.featuresToLoad = features;
    pref.numFeaturesToLoad = 1u;
    pref.renderAPI = sl::RenderAPI::eVulkan;
    pref.flags |= sl::PreferenceFlags::eUseManualHooking;
    pref.flags |= sl::PreferenceFlags::eUseFrameBasedResourceTagging;
    pref.logMessageCallback = SL_LoggingCallback;
    pref.logLevel = sl::LogLevel::eOff;
    const std::wstring localPluginPath = GetAbsolutePath(L".");
    const std::wstring externalPluginPath = GetAbsolutePath(L"External/additional_libs/NVStreamline");
    const wchar_t* pluginPaths[] = {localPluginPath.c_str(), externalPluginPath.c_str()};
    if (DirectoryExists(externalPluginPath.c_str()))
    {
        pref.pathsToPlugins = pluginPaths;
        pref.numPathsToPlugins = 2;
    }

    pref.engine = sl::EngineType::eCustom;
    pref.engineVersion = "1.0.0";
    pref.projectId = "2f0f5f9e-13d4-4d63-bb9b-06f7f8cf1d5f";

    sl::Result res = slInit(pref, sl::kSDKVersion);
    if (res != sl::Result::eOk)
    {
        DEBUG_LOG_WARNF("[StreamlineManager] slInit failed with error: {}", static_cast<int>(res));
        ReleaseStreamlineLibrary();
        return false;
    }

    g_slInitialized = true;
    return true;
}

bool StreamlineManager::Init()
{
    if (s_initialized)
        return true;
    if (!g_slInitialized)
        return false;

    const auto& context = VkGlobals::GetContext();
    const auto queueFamilies = VkGlobals::GetQueueFamilyIndices();

    sl::VulkanInfo vulkanInfo{};
    vulkanInfo.instance = context.Instance;
    vulkanInfo.device = context.Device;
    vulkanInfo.physicalDevice = context.PhysicalDevice;
    vulkanInfo.graphicsQueueFamily = queueFamilies.graphicsFamily.value_or(0);
    vulkanInfo.computeQueueFamily = queueFamilies.computeFamily.value_or(0);
    vulkanInfo.graphicsQueueIndex = g_slGraphicsQueueStartIndex;
    vulkanInfo.computeQueueIndex = g_slComputeQueueStartIndex;
    sl::Result res = slSetVulkanInfo(vulkanInfo);
    if (res != sl::Result::eOk)
    {
        DEBUG_LOG_WARNF("[StreamlineManager] slSetVulkanInfo failed with error: {}", static_cast<int>(res));
        Shutdown();
        return false;
    }

    // Check support
    sl::AdapterInfo adapterInfo{};
    adapterInfo.vkPhysicalDevice = context.PhysicalDevice;
    res = slIsFeatureSupported(sl::kFeatureDLSS, adapterInfo);
    g_dlssSupported = res == sl::Result::eOk;
    if (res != sl::Result::eOk)
    {
        DEBUG_LOG_WARNF("[StreamlineManager] DLSS feature unsupported on selected adapter: {}", static_cast<int>(res));
        Shutdown();
        return false;
    }

    sl::FeatureRequirements dlssRequirements{};
    if (GetDLSSFeatureRequirements(dlssRequirements))
    {
        DEBUG_LOGF(
            "[StreamlineManager] DLSS requirements: gfxQueues={}, computeQueues={}, instanceExts={}, deviceExts={}",
            dlssRequirements.vkNumGraphicsQueuesRequired,
            dlssRequirements.vkNumComputeQueuesRequired,
            dlssRequirements.vkNumInstanceExtensions,
            dlssRequirements.vkNumDeviceExtensions);
    }

    if (g_imguiPluginAvailable)
    {
        const sl::Result imguiRes = slIsFeatureSupported(sl::kFeatureImGUI, adapterInfo);
        g_imguiSupported = imguiRes == sl::Result::eOk;
        if (imguiRes != sl::Result::eOk)
            DEBUG_LOG_WARNF("[StreamlineManager] Streamline ImGui feature unsupported: {}", static_cast<int>(imguiRes));
    }

    s_initialized = true;
    auto debugState = GetDLSSDebugState();
    debugState.streamlineInitialized = true;
    debugState.featureSupported = g_dlssSupported;
    debugState.imguiPluginAvailable = g_imguiPluginAvailable;
    SetDLSSDebugState(debugState);
    return true;
}

void StreamlineManager::Shutdown()
{
    if (g_slInitialized)
    {
        slShutdown();
    }
    s_initialized = false;
    g_slInitialized = false;
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
    g_dlssSupported = false;
    g_imguiPluginAvailable = false;
    g_imguiSupported = false;
    g_slGraphicsQueueStartIndex = 0;
    g_slComputeQueueStartIndex = 0;
    ReleaseStreamlineLibrary();
}

bool StreamlineManager::GetFrameToken(u32 frameIdx, sl::FrameToken*& pFrameToken)
{
    (void)frameIdx;
    if (!s_initialized)
        return false;
    return slGetNewFrameToken(pFrameToken, nullptr) == sl::Result::eOk && pFrameToken != nullptr;
}

bool StreamlineManager::GetDLSSFeatureRequirements(sl::FeatureRequirements& requirements)
{
    if (!g_slInitialized)
        return false;
    const sl::Result res = slGetFeatureRequirements(sl::kFeatureDLSS, requirements);
    if (res != sl::Result::eOk)
    {
        DEBUG_LOG_WARNF("[StreamlineManager] slGetFeatureRequirements(DLSS) failed with result: 0x{:X}",
                        static_cast<u32>(res));
        return false;
    }
    return true;
}

bool StreamlineManager::GetDLSSOptimalSettings(u32 width,
                                               u32 height,
                                               sl::DLSSMode mode,
                                               sl::DLSSOptimalSettings& settings)
{
    if (!s_initialized)
        return false;
    sl::DLSSOptions options{};
    options.mode = mode;
    options.outputWidth = width;
    options.outputHeight = height;
    return slDLSSGetOptimalSettings(options, settings) == sl::Result::eOk;
}

bool StreamlineManager::GetDLSSState(sl::DLSSState& state)
{
    if (!s_initialized)
        return false;

    sl::ViewportHandle viewport(0);
    return slDLSSGetState(viewport, state) == sl::Result::eOk;
}

VkResult StreamlineManager::CreateVulkanInstance(const VkInstanceCreateInfo* pCreateInfo,
                                                 const VkAllocationCallbacks* pAllocator,
                                                 VkInstance* pInstance)
{
    if (g_slInitialized && g_slVkCreateInstance)
        return g_slVkCreateInstance(pCreateInfo, pAllocator, pInstance);
    return vkCreateInstance(pCreateInfo, pAllocator, pInstance);
}

VkResult StreamlineManager::CreateVulkanDevice(VkPhysicalDevice physicalDevice,
                                               const VkDeviceCreateInfo* pCreateInfo,
                                               const VkAllocationCallbacks* pAllocator,
                                               VkDevice* pDevice)
{
    return vkCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
}

void StreamlineManager::SetVulkanQueueStartIndices(u32 graphicsQueueIndex, u32 computeQueueIndex)
{
    g_slGraphicsQueueStartIndex = graphicsQueueIndex;
    g_slComputeQueueStartIndex = computeQueueIndex;
}

void StreamlineManager::GetVulkanDeviceQueue(VkDevice device, u32 queueFamilyIndex, u32 queueIndex, VkQueue* pQueue)
{
    static PFN_vkGetDeviceQueue s_pStreamlineProc = nullptr;
    const auto pStreamlineProc = ResolveStreamlineDeviceProc(device, "vkGetDeviceQueue", s_pStreamlineProc);
    if (pStreamlineProc)
    {
        pStreamlineProc(device, queueFamilyIndex, queueIndex, pQueue);
        return;
    }

    const auto pLoaderFn = reinterpret_cast<PFN_vkGetDeviceQueue>(vkGetDeviceProcAddr(device, "vkGetDeviceQueue"));
    if (pLoaderFn)
    {
        pLoaderFn(device, queueFamilyIndex, queueIndex, pQueue);
        return;
    }

    *pQueue = VK_NULL_HANDLE;
}

bool StreamlineManager::SetDLSSOptions(u32 width, u32 height, sl::DLSSMode mode)
{
    if (!s_initialized)
        return false;

    sl::DLSSOptions options{};
    options.mode = mode;
    options.outputWidth = width;
    options.outputHeight = height;
    options.colorBuffersHDR = sl::Boolean::eTrue;

    sl::ViewportHandle viewport(0);
    const auto res = slDLSSSetOptions(viewport, options);
    if (res != sl::Result::eOk)
    {
        DEBUG_LOG_WARNF("[StreamlineManager] SetDLSSOptions failed with result: 0x{:X}", static_cast<u32>(res));
    }
    return res == sl::Result::eOk;
}

bool StreamlineManager::EnsureDLSSConfigured(u32 width, u32 height, sl::DLSSMode mode)
{
    if (!s_initialized || width == 0 || height == 0)
        return false;

    const bool configChanged =
        !g_dlssConfigured || g_dlssWidth != width || g_dlssHeight != height || g_dlssMode != mode;
    const bool sameAsLastFailedConfig = g_dlssLastConfigureFailed && g_dlssFailedWidth == width &&
                                        g_dlssFailedHeight == height && g_dlssFailedMode == mode;
    if (!configChanged)
        return true;
    if (sameAsLastFailedConfig)
        return false;

    if (g_dlssConfigured && !FreeResources(sl::kFeatureDLSS))
    {
        DEBUG_LOG_WARN("[StreamlineManager] Failed to free old DLSS resources during reconfigure");
    }
    g_dlssConfigured = false;

    if (!SetDLSSOptions(width, height, mode))
    {
        DEBUG_LOG_WARNF("[StreamlineManager] slDLSSSetOptions failed ({}x{})", width, height);
        g_dlssLastConfigureFailed = true;
        g_dlssFailedWidth = width;
        g_dlssFailedHeight = height;
        g_dlssFailedMode = mode;
        auto debugState = GetDLSSDebugState();
        debugState.streamlineInitialized = s_initialized;
        debugState.featureSupported = g_dlssSupported;
        debugState.imguiPluginAvailable = g_imguiPluginAvailable;
        debugState.configured = false;
        debugState.lastConfigureFailed = true;
        debugState.evaluateBlocked = g_dlssEvaluateBlocked;
        debugState.needsReset = g_dlssNeedsReset;
        debugState.configuredMode = mode;
        debugState.outputWidth = width;
        debugState.outputHeight = height;
        SetDLSSDebugState(debugState);
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

    auto debugState = GetDLSSDebugState();
    debugState.streamlineInitialized = s_initialized;
    debugState.featureSupported = g_dlssSupported;
    debugState.imguiPluginAvailable = g_imguiPluginAvailable;
    debugState.configured = true;
    debugState.evaluateBlocked = g_dlssEvaluateBlocked;
    debugState.lastConfigureFailed = false;
    debugState.needsReset = g_dlssNeedsReset;
    debugState.configuredMode = mode;
    debugState.outputWidth = width;
    debugState.outputHeight = height;

    sl::DLSSOptimalSettings optimalSettings{};
    if (GetDLSSOptimalSettings(width, height, mode, optimalSettings))
    {
        debugState.optimalRenderWidth = optimalSettings.optimalRenderWidth;
        debugState.optimalRenderHeight = optimalSettings.optimalRenderHeight;
        debugState.optimalSharpness = optimalSettings.optimalSharpness;
        debugState.renderWidthMin = optimalSettings.renderWidthMin;
        debugState.renderHeightMin = optimalSettings.renderHeightMin;
        debugState.renderWidthMax = optimalSettings.renderWidthMax;
        debugState.renderHeightMax = optimalSettings.renderHeightMax;
    }

    SetDLSSDebugState(debugState);
    return true;
}

bool StreamlineManager::ConsumeDLSSResetFlag()
{
    const bool needsReset = g_dlssNeedsReset;
    g_dlssNeedsReset = false;
    auto debugState = GetDLSSDebugState();
    debugState.needsReset = g_dlssNeedsReset;
    SetDLSSDebugState(debugState);
    return needsReset;
}

bool StreamlineManager::FreeResources(sl::Feature feature)
{
    if (!s_initialized)
        return false;
    sl::ViewportHandle viewport(0);
    const auto res = slFreeResources(feature, viewport);
    return res == sl::Result::eOk;
}

sl::Result StreamlineManager::SetTagForFrame(const sl::FrameToken& frame,
                                             const sl::ViewportHandle& viewport,
                                             const sl::ResourceTag* tags,
                                             uint32_t numTags,
                                             sl::CommandBuffer* cmdBuffer)
{
    if (!s_initialized)
        return sl::Result::eErrorNotInitialized;
    return slSetTagForFrame(frame, viewport, tags, numTags, cmdBuffer);
}

sl::Result StreamlineManager::SetConstants(const sl::Constants& values,
                                           const sl::FrameToken& frame,
                                           const sl::ViewportHandle& viewport)
{
    if (!s_initialized)
        return sl::Result::eErrorNotInitialized;
    return slSetConstants(values, frame, viewport);
}

bool StreamlineManager::EvaluateDLSS(VkCommandBuffer cmdBuf, const sl::FrameToken& frameToken)
{
    if (!s_initialized)
        return false;
    if (g_dlssEvaluateBlocked)
        return false;

    sl::ViewportHandle viewport(0);
    const sl::BaseStructure* inputs[] = {&viewport};
    const auto res = slEvaluateFeature(sl::kFeatureDLSS, frameToken, inputs, 1, (sl::CommandBuffer*)cmdBuf);
    if (res != sl::Result::eOk)
    {
        DEBUG_LOG_WARNF("[StreamlineManager] EvaluateDLSS failed with result: 0x{:X}", static_cast<u32>(res));
        if (res == sl::Result::eErrorNGXFailed)
        {
            g_dlssEvaluateBlocked = true;
            DEBUG_LOG_WARN("[StreamlineManager] Blocking further DLSS evaluate calls for current config to prevent NGX "
                           "recreation/VRAM growth");
        }
    }
    auto debugState = GetDLSSDebugState();
    debugState.lastEvaluateResult = res;
    debugState.evaluateBlocked = g_dlssEvaluateBlocked;
    ++debugState.evaluateCallCount;
    sl::DLSSState dlssState{};
    if (GetDLSSState(dlssState))
        debugState.estimatedVRAMUsageInBytes = dlssState.estimatedVRAMUsageInBytes;
    SetDLSSDebugState(debugState);
    return res == sl::Result::eOk;
}

bool StreamlineManager::IsDLSSEvaluateBlocked()
{
    return g_dlssEvaluateBlocked;
}

bool StreamlineManager::IsDLSSSupported()
{
    return s_initialized && g_dlssSupported;
}

bool StreamlineManager::IsDLSSDebugUIAvailable()
{
    return s_initialized && g_imguiSupported;
}

StreamlineManager::DLSSDebugState StreamlineManager::GetDLSSDebugState()
{
    SimpleScopedGuard lock(g_dlssDebugStateMutex);
    return g_dlssDebugState;
}

void StreamlineManager::SetDLSSDebugState(const DLSSDebugState& state)
{
    SimpleScopedGuard lock(g_dlssDebugStateMutex);
    g_dlssDebugState = state;
}

bool StreamlineManager::AllocateResources(VkCommandBuffer cmdBuf, sl::Feature feature)
{
    if (!s_initialized)
        return false;
    sl::ViewportHandle viewport(0);
    const auto res = slAllocateResources((sl::CommandBuffer*)cmdBuf, feature, viewport);
    if (res != sl::Result::eOk)
    {
        DEBUG_LOG_WARNF("[StreamlineManager] AllocateResources failed with result: 0x{:X}", static_cast<u32>(res));
    }
    return res == sl::Result::eOk;
}

} // namespace Nvidia

extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char* pName)
{
    if (Nvidia::g_slVkGetInstanceProcAddr)
        return Nvidia::g_slVkGetInstanceProcAddr(instance, pName);

    const auto pNativeProc = Nvidia::GetNativeVkGetInstanceProcAddr();
    return pNativeProc ? pNativeProc(instance, pName) : nullptr;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice device,
                                                               const VkSwapchainCreateInfoKHR* pCreateInfo,
                                                               const VkAllocationCallbacks* pAllocator,
                                                               VkSwapchainKHR* pSwapchain)
{
    static PFN_vkCreateSwapchainKHR s_pStreamlineProc = nullptr;
    const auto pStreamlineProc = Nvidia::ResolveStreamlineDeviceProc(device, "vkCreateSwapchainKHR", s_pStreamlineProc);
    return pStreamlineProc ? pStreamlineProc(device, pCreateInfo, pAllocator, pSwapchain)
                           : VK_ERROR_EXTENSION_NOT_PRESENT;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroySwapchainKHR(VkDevice device,
                                                            VkSwapchainKHR swapchain,
                                                            const VkAllocationCallbacks* pAllocator)
{
    static PFN_vkDestroySwapchainKHR s_pStreamlineProc = nullptr;
    const auto pStreamlineProc = Nvidia::ResolveStreamlineDeviceProc(device, "vkDestroySwapchainKHR", s_pStreamlineProc);
    if (pStreamlineProc)
        pStreamlineProc(device, swapchain, pAllocator);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetSwapchainImagesKHR(VkDevice device,
                                                                  VkSwapchainKHR swapchain,
                                                                  uint32_t* pSwapchainImageCount,
                                                                  VkImage* pSwapchainImages)
{
    static PFN_vkGetSwapchainImagesKHR s_pStreamlineProc = nullptr;
    const auto pStreamlineProc =
        Nvidia::ResolveStreamlineDeviceProc(device, "vkGetSwapchainImagesKHR", s_pStreamlineProc);
    return pStreamlineProc ? pStreamlineProc(device, swapchain, pSwapchainImageCount, pSwapchainImages)
                           : VK_ERROR_EXTENSION_NOT_PRESENT;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImageKHR(VkDevice device,
                                                                VkSwapchainKHR swapchain,
                                                                uint64_t timeout,
                                                                VkSemaphore semaphore,
                                                                VkFence fence,
                                                                uint32_t* pImageIndex)
{
    static PFN_vkAcquireNextImageKHR s_pStreamlineProc = nullptr;
    const auto pStreamlineProc = Nvidia::ResolveStreamlineDeviceProc(device, "vkAcquireNextImageKHR", s_pStreamlineProc);
    const u64 callCount = ++Nvidia::g_slAcquireCallCount;
    if (!pStreamlineProc)
    {
        DEBUG_LOG_WARN("[StreamlineManager] Streamline acquire proxy missing");
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    const VkResult result = pStreamlineProc(device, swapchain, timeout, semaphore, fence, pImageIndex);
    return result;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateWin32SurfaceKHR(
    VkInstance instance,
    const void* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkSurfaceKHR* pSurface)
{
    using PFN_vkCreateWin32SurfaceKHRCompat =
        VkResult(VKAPI_PTR*)(VkInstance, const void*, const VkAllocationCallbacks*, VkSurfaceKHR*);
    static PFN_vkCreateWin32SurfaceKHRCompat s_pStreamlineProc = nullptr;
    const auto pStreamlineProc =
        Nvidia::ResolveStreamlineInstanceProc(instance, "vkCreateWin32SurfaceKHR", s_pStreamlineProc);
    return pStreamlineProc ? pStreamlineProc(instance, pCreateInfo, pAllocator, pSurface)
                           : VK_ERROR_EXTENSION_NOT_PRESENT;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroySurfaceKHR(VkInstance instance,
                                                          VkSurfaceKHR surface,
                                                          const VkAllocationCallbacks* pAllocator)
{
    static PFN_vkDestroySurfaceKHR s_pStreamlineProc = nullptr;
    const auto pStreamlineProc =
        Nvidia::ResolveStreamlineInstanceProc(instance, "vkDestroySurfaceKHR", s_pStreamlineProc);
    if (pStreamlineProc)
        pStreamlineProc(instance, surface, pAllocator);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo)
{
    static PFN_vkQueuePresentKHR s_pStreamlineProc = nullptr;
    const auto pStreamlineProc =
        Nvidia::ResolveStreamlineDeviceProc(VkGlobals::GetLogicalDevice(), "vkQueuePresentKHR", s_pStreamlineProc);
    const u64 callCount = ++Nvidia::g_slPresentCallCount;
    if (!pStreamlineProc)
    {
        DEBUG_LOG_WARN("[StreamlineManager] Streamline present proxy missing");
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    const VkResult result = pStreamlineProc(queue, pPresentInfo);
    return result;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice device)
{
    static PFN_vkDeviceWaitIdle s_pStreamlineProc = nullptr;
    const auto pStreamlineProc = Nvidia::ResolveStreamlineDeviceProc(device, "vkDeviceWaitIdle", s_pStreamlineProc);
    return pStreamlineProc ? pStreamlineProc(device) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue,
                                                        uint32_t submitCount,
                                                        const VkSubmitInfo* pSubmits,
                                                        VkFence fence)
{
    static PFN_vkQueueSubmit s_pStreamlineProc = nullptr;
    const auto pStreamlineProc =
        Nvidia::ResolveStreamlineDeviceProc(VkGlobals::GetLogicalDevice(), "vkQueueSubmit", s_pStreamlineProc);
    return pStreamlineProc ? pStreamlineProc(queue, submitCount, pSubmits, fence) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit2(VkQueue queue,
                                                         uint32_t submitCount,
                                                         const VkSubmitInfo2* pSubmits,
                                                         VkFence fence)
{
    static PFN_vkQueueSubmit2 s_pStreamlineProc = nullptr;
    const auto pStreamlineProc =
        Nvidia::ResolveStreamlineDeviceProc(VkGlobals::GetLogicalDevice(), "vkQueueSubmit2", s_pStreamlineProc);
    return pStreamlineProc ? pStreamlineProc(queue, submitCount, pSubmits, fence) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer commandBuffer,
                                                               const VkCommandBufferBeginInfo* pBeginInfo)
{
    static PFN_vkBeginCommandBuffer s_pStreamlineProc = nullptr;
    const auto pStreamlineProc =
        Nvidia::ResolveStreamlineDeviceProc(VkGlobals::GetLogicalDevice(), "vkBeginCommandBuffer", s_pStreamlineProc);
    return pStreamlineProc ? pStreamlineProc(commandBuffer, pBeginInfo) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateCommandPool(VkDevice device,
                                                              const VkCommandPoolCreateInfo* pCreateInfo,
                                                              const VkAllocationCallbacks* pAllocator,
                                                              VkCommandPool* pCommandPool)
{
    static PFN_vkCreateCommandPool s_pStreamlineProc = nullptr;
    const auto pStreamlineProc = Nvidia::ResolveStreamlineDeviceProc(device, "vkCreateCommandPool", s_pStreamlineProc);
    return pStreamlineProc ? pStreamlineProc(device, pCreateInfo, pAllocator, pCommandPool)
                           : VK_ERROR_EXTENSION_NOT_PRESENT;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer commandBuffer,
                                                        VkPipelineBindPoint pipelineBindPoint,
                                                        VkPipeline pipeline)
{
    static PFN_vkCmdBindPipeline s_pStreamlineProc = nullptr;
    const auto pStreamlineProc =
        Nvidia::ResolveStreamlineDeviceProc(VkGlobals::GetLogicalDevice(), "vkCmdBindPipeline", s_pStreamlineProc);
    if (pStreamlineProc)
        pStreamlineProc(commandBuffer, pipelineBindPoint, pipeline);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorSets(VkCommandBuffer commandBuffer,
                                                              VkPipelineBindPoint pipelineBindPoint,
                                                              VkPipelineLayout layout,
                                                              uint32_t firstSet,
                                                              uint32_t descriptorSetCount,
                                                              const VkDescriptorSet* pDescriptorSets,
                                                              uint32_t dynamicOffsetCount,
                                                              const uint32_t* pDynamicOffsets)
{
    static PFN_vkCmdBindDescriptorSets s_pStreamlineProc = nullptr;
    const auto pStreamlineProc = Nvidia::ResolveStreamlineDeviceProc(
        VkGlobals::GetLogicalDevice(), "vkCmdBindDescriptorSets", s_pStreamlineProc);
    if (pStreamlineProc)
    {
        pStreamlineProc(commandBuffer,
                        pipelineBindPoint,
                        layout,
                        firstSet,
                        descriptorSetCount,
                        pDescriptorSets,
                        dynamicOffsetCount,
                        pDynamicOffsets);
    }
}
