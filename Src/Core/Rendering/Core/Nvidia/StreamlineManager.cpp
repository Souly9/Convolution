#include "StreamlineManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Global/ThreadBase.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include <sl_helpers_vk.h>
#include <string>
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
static HMODULE g_slModule = nullptr;
static ProfiledLockable(CustomMutex, g_dlssDebugStateMutex);
static StreamlineManager::DLSSDebugState g_dlssDebugState{};

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
static PFun_slDLSSGetOptimalSettings* g_slDLSSGetOptimalSettings = nullptr;
static PFun_slDLSSGetState* g_slDLSSGetState = nullptr;
static PFun_slDLSSSetOptions* g_slDLSSSetOptions = nullptr;
static PFN_vkGetDeviceProcAddr g_slVkGetDeviceProcAddr = nullptr;
static u32 g_slGraphicsQueueStartIndex = 0;
static u32 g_slComputeQueueStartIndex = 0;

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

static void ResetStreamlineFunctionPointers()
{
    g_slInit = nullptr;
    g_slShutdown = nullptr;
    g_slGetFeatureFunction = nullptr;
    g_slGetNewFrameToken = nullptr;
    g_slSetVulkanInfo = nullptr;
    g_slSetTagForFrame = nullptr;
    g_slSetConstants = nullptr;
    g_slEvaluateFeature = nullptr;
    g_slIsFeatureSupported = nullptr;
    g_slGetFeatureRequirements = nullptr;
    g_slAllocateResources = nullptr;
    g_slFreeResources = nullptr;
    g_slDLSSGetOptimalSettings = nullptr;
    g_slDLSSGetState = nullptr;
    g_slDLSSSetOptions = nullptr;
    g_slVkGetDeviceProcAddr = nullptr;
}

static void ReleaseStreamlineLibrary()
{
    if (g_slModule)
    {
        FreeLibrary(g_slModule);
        g_slModule = nullptr;
    }
    ResetStreamlineFunctionPointers();
}

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

template <typename T>
static bool ResolveFeatureFunction(sl::Feature feature, const char* name, T*& pCachedFn)
{
    if (pCachedFn)
        return true;
    if (!g_slGetFeatureFunction)
        return false;

    void* pFunction = nullptr;
    const sl::Result res = g_slGetFeatureFunction(feature, name, pFunction);
    if (res != sl::Result::eOk || !pFunction)
        return false;

    pCachedFn = reinterpret_cast<T*>(pFunction);
    return true;
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

    // Try loading from local directory first
    g_slModule = LoadLibraryA("sl.interposer.dll");
    if (!g_slModule)
    {
        g_slModule = LoadLibraryA("External/additional_libs/NVStreamline/sl.interposer.dll");
    }

    if (!g_slModule)
    {
        DEBUG_LOG_WARN("[StreamlineManager] Failed to load sl.interposer.dll");
        ReleaseStreamlineLibrary();
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

    if (!g_slInit || !g_slShutdown || !g_slGetFeatureFunction || !g_slGetNewFrameToken || !g_slSetVulkanInfo ||
        !g_slSetTagForFrame || !g_slSetConstants || !g_slEvaluateFeature || !g_slIsFeatureSupported ||
        !g_slGetFeatureRequirements || !g_slAllocateResources || !g_slFreeResources || !g_slVkGetDeviceProcAddr)
    {
        DEBUG_LOG_WARN("[StreamlineManager] Failed to resolve function pointers");
        ReleaseStreamlineLibrary();
        return false;
    }

    // Call slInit EARLY so interposer can hook Vulkan creation
    sl::Preferences pref{};
    g_imguiPluginAvailable = FileExists("sl.imgui.dll") ||
                             FileExists("External/additional_libs/NVStreamline/sl.imgui.dll");
    sl::Feature features[] = {sl::kFeatureDLSS, sl::kFeatureImGUI};
    pref.featuresToLoad = features;
    pref.numFeaturesToLoad = g_imguiPluginAvailable ? 2u : 1u;
    pref.renderAPI = sl::RenderAPI::eVulkan;
    pref.flags |= sl::PreferenceFlags::eUseFrameBasedResourceTagging;
    pref.flags |= sl::PreferenceFlags::eUseManualHooking;
    pref.logMessageCallback = SL_LoggingCallback;
    pref.logLevel = sl::LogLevel::eVerbose;
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

    sl::Result res = g_slInit(pref, sl::kSDKVersion);
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
    if (!g_slSetVulkanInfo)
        return false;

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
        DEBUG_LOG_WARNF("[StreamlineManager] slSetVulkanInfo failed with error: {}", static_cast<int>(res));
        Shutdown();
        return false;
    }

    // Check support
    sl::AdapterInfo adapterInfo{};
    adapterInfo.vkPhysicalDevice = context.PhysicalDevice;
    res = g_slIsFeatureSupported(sl::kFeatureDLSS, adapterInfo);
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
    if (g_slInitialized && g_slShutdown)
    {
        g_slShutdown();
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
    g_slGraphicsQueueStartIndex = 0;
    g_slComputeQueueStartIndex = 0;
    SetDLSSDebugState(DLSSDebugState{});
    ReleaseStreamlineLibrary();
}

bool StreamlineManager::GetFrameToken(u32 frameIdx, sl::FrameToken*& pFrameToken)
{
    (void)frameIdx;
    if (!s_initialized || !g_slGetNewFrameToken)
        return false;
    return g_slGetNewFrameToken(pFrameToken, nullptr) == sl::Result::eOk && pFrameToken != nullptr;
}

bool StreamlineManager::GetDLSSFeatureRequirements(sl::FeatureRequirements& requirements)
{
    if (!g_slGetFeatureRequirements)
        return false;
    const sl::Result res = g_slGetFeatureRequirements(sl::kFeatureDLSS, requirements);
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
    if (!s_initialized ||
        !ResolveFeatureFunction(sl::kFeatureDLSS, "slDLSSGetOptimalSettings", g_slDLSSGetOptimalSettings))
        return false;
    sl::DLSSOptions options{};
    options.mode = mode;
    options.outputWidth = width;
    options.outputHeight = height;
    return g_slDLSSGetOptimalSettings(options, settings) == sl::Result::eOk;
}

bool StreamlineManager::GetDLSSState(sl::DLSSState& state)
{
    if (!s_initialized || !ResolveFeatureFunction(sl::kFeatureDLSS, "slDLSSGetState", g_slDLSSGetState))
        return false;

    sl::ViewportHandle viewport(0);
    return g_slDLSSGetState(viewport, state) == sl::Result::eOk;
}

void StreamlineManager::GetVulkanDeviceQueue(VkDevice device, u32 queueFamilyIndex, u32 queueIndex, VkQueue* pQueue)
{
    static PFN_vkGetDeviceQueue s_pHook = nullptr;
    const auto pHook = ResolveDeviceHook(device, "vkGetDeviceQueue", s_pHook);
    if (pHook)
    {
        pHook(device, queueFamilyIndex, queueIndex, pQueue);
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

void StreamlineManager::GetVulkanDeviceQueue2(VkDevice device, const VkDeviceQueueInfo2* pQueueInfo, VkQueue* pQueue)
{
    static PFN_vkGetDeviceQueue2 s_pHook = nullptr;
    const auto pHook = ResolveDeviceHook(device, "vkGetDeviceQueue2", s_pHook);
    if (pHook)
    {
        pHook(device, pQueueInfo, pQueue);
        return;
    }

    const auto pLoaderFn = reinterpret_cast<PFN_vkGetDeviceQueue2>(vkGetDeviceProcAddr(device, "vkGetDeviceQueue2"));
    if (pLoaderFn)
    {
        pLoaderFn(device, pQueueInfo, pQueue);
        return;
    }

    *pQueue = VK_NULL_HANDLE;
}

void StreamlineManager::SetVulkanQueueStartIndices(u32 graphicsQueueIndex, u32 computeQueueIndex)
{
    g_slGraphicsQueueStartIndex = graphicsQueueIndex;
    g_slComputeQueueStartIndex = computeQueueIndex;
}

bool StreamlineManager::SetDLSSOptions(u32 width, u32 height, sl::DLSSMode mode)
{
    if (!s_initialized || !ResolveFeatureFunction(sl::kFeatureDLSS, "slDLSSSetOptions", g_slDLSSSetOptions))
        return false;

    sl::DLSSOptions options{};
    options.mode = mode;
    options.outputWidth = width;
    options.outputHeight = height;
    options.colorBuffersHDR = sl::Boolean::eTrue;

    sl::ViewportHandle viewport(0);
    const auto res = g_slDLSSSetOptions(viewport, options);
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
    if (!s_initialized || !g_slFreeResources)
        return false;
    sl::ViewportHandle viewport(0);
    const auto res = g_slFreeResources(feature, viewport);
    return res == sl::Result::eOk;
}

sl::Result StreamlineManager::SetTagForFrame(const sl::FrameToken& frame,
                                             const sl::ViewportHandle& viewport,
                                             const sl::ResourceTag* tags,
                                             uint32_t numTags,
                                             sl::CommandBuffer* cmdBuffer)
{
    if (!s_initialized || !g_slSetTagForFrame)
        return sl::Result::eErrorNotInitialized;
    return g_slSetTagForFrame(frame, viewport, tags, numTags, cmdBuffer);
}

sl::Result StreamlineManager::SetConstants(const sl::Constants& values,
                                           const sl::FrameToken& frame,
                                           const sl::ViewportHandle& viewport)
{
    if (!s_initialized || !g_slSetConstants)
        return sl::Result::eErrorNotInitialized;
    return g_slSetConstants(values, frame, viewport);
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
    return g_imguiPluginAvailable;
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
    if (!s_initialized || !g_slAllocateResources)
        return false;
    sl::ViewportHandle viewport(0);
    const auto res = g_slAllocateResources((sl::CommandBuffer*)cmdBuf, feature, viewport);
    if (res != sl::Result::eOk)
    {
        DEBUG_LOG_WARNF("[StreamlineManager] AllocateResources failed with result: 0x{:X}", static_cast<u32>(res));
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

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateCommandPool(VkDevice device,
                                                              const VkCommandPoolCreateInfo* pCreateInfo,
                                                              const VkAllocationCallbacks* pAllocator,
                                                              VkCommandPool* pCommandPool)
{
    static PFN_vkCreateCommandPool s_pHook = nullptr;
    const auto pHook = Nvidia::ResolveDeviceHook(device, "vkCreateCommandPool", s_pHook);
    return pHook ? pHook(device, pCreateInfo, pAllocator, pCommandPool) : VK_ERROR_INITIALIZATION_FAILED;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetDeviceQueue(VkDevice device,
                                                       uint32_t queueFamilyIndex,
                                                       uint32_t queueIndex,
                                                       VkQueue* pQueue)
{
    Nvidia::StreamlineManager::GetVulkanDeviceQueue(device, queueFamilyIndex, queueIndex, pQueue);
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkGetDeviceQueue2(VkDevice device,
                                                        const VkDeviceQueueInfo2* pQueueInfo,
                                                        VkQueue* pQueue)
{
    Nvidia::StreamlineManager::GetVulkanDeviceQueue2(device, pQueueInfo, pQueue);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer commandBuffer,
                                                               const VkCommandBufferBeginInfo* pBeginInfo)
{
    static PFN_vkBeginCommandBuffer s_pHook = nullptr;
    const auto pHook = Nvidia::ResolveDeviceHook(VkGlobals::GetLogicalDevice(), "vkBeginCommandBuffer", s_pHook);
    return pHook ? pHook(commandBuffer, pBeginInfo) : VK_ERROR_INITIALIZATION_FAILED;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer commandBuffer,
                                                        VkPipelineBindPoint pipelineBindPoint,
                                                        VkPipeline pipeline)
{
    static PFN_vkCmdBindPipeline s_pHook = nullptr;
    const auto pHook = Nvidia::ResolveDeviceHook(VkGlobals::GetLogicalDevice(), "vkCmdBindPipeline", s_pHook);
    if (pHook)
        pHook(commandBuffer, pipelineBindPoint, pipeline);
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
    static PFN_vkCmdBindDescriptorSets s_pHook = nullptr;
    const auto pHook = Nvidia::ResolveDeviceHook(VkGlobals::GetLogicalDevice(), "vkCmdBindDescriptorSets", s_pHook);
    if (pHook)
    {
        pHook(commandBuffer,
              pipelineBindPoint,
              layout,
              firstSet,
              descriptorSetCount,
              pDescriptorSets,
              dynamicOffsetCount,
              pDynamicOffsets);
    }
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue,
                                                        uint32_t submitCount,
                                                        const VkSubmitInfo* pSubmits,
                                                        VkFence fence)
{
    static PFN_vkQueueSubmit s_pHook = nullptr;
    const auto pHook = Nvidia::ResolveDeviceHook(VkGlobals::GetLogicalDevice(), "vkQueueSubmit", s_pHook);
    return pHook ? pHook(queue, submitCount, pSubmits, fence) : VK_ERROR_INITIALIZATION_FAILED;
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit2(VkQueue queue,
                                                         uint32_t submitCount,
                                                         const VkSubmitInfo2* pSubmits,
                                                         VkFence fence)
{
    static PFN_vkQueueSubmit2 s_pHook = nullptr;
    const auto pHook = Nvidia::ResolveDeviceHook(VkGlobals::GetLogicalDevice(), "vkQueueSubmit2", s_pHook);
    return pHook ? pHook(queue, submitCount, pSubmits, fence) : VK_ERROR_INITIALIZATION_FAILED;
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
extern "C"
{
    sl::Result slInit(const sl::Preferences& pref, uint64_t sdkVersion)
    {
        return Nvidia::g_slInit ? Nvidia::g_slInit(pref, sdkVersion) : sl::Result::eErrorNotInitialized;
    }
    sl::Result slShutdown()
    {
        return Nvidia::g_slShutdown ? Nvidia::g_slShutdown() : sl::Result::eErrorNotInitialized;
    }
    sl::Result slGetFeatureFunction(sl::Feature feature, const char* functionName, void*& function)
    {
        return Nvidia::g_slGetFeatureFunction ? Nvidia::g_slGetFeatureFunction(feature, functionName, function)
                                              : sl::Result::eErrorNotInitialized;
    }
    sl::Result slGetNewFrameToken(sl::FrameToken*& token, const uint32_t* frameIndex)
    {
        return Nvidia::g_slGetNewFrameToken ? Nvidia::g_slGetNewFrameToken(token, frameIndex)
                                            : sl::Result::eErrorNotInitialized;
    }
    sl::Result slSetVulkanInfo(const sl::VulkanInfo& info)
    {
        return Nvidia::g_slSetVulkanInfo ? Nvidia::g_slSetVulkanInfo(info) : sl::Result::eErrorNotInitialized;
    }
    sl::Result slSetTagForFrame(const sl::FrameToken& frame,
                                const sl::ViewportHandle& viewport,
                                const sl::ResourceTag* tags,
                                uint32_t numTags,
                                sl::CommandBuffer* cmdBuffer)
    {
        return Nvidia::g_slSetTagForFrame ? Nvidia::g_slSetTagForFrame(frame, viewport, tags, numTags, cmdBuffer)
                                          : sl::Result::eErrorNotInitialized;
    }
    sl::Result slSetConstants(const sl::Constants& values,
                              const sl::FrameToken& frame,
                              const sl::ViewportHandle& viewport)
    {
        return Nvidia::g_slSetConstants ? Nvidia::g_slSetConstants(values, frame, viewport)
                                        : sl::Result::eErrorNotInitialized;
    }
    sl::Result slEvaluateFeature(sl::Feature feature,
                                 const sl::FrameToken& frame,
                                 const sl::BaseStructure** inputs,
                                 uint32_t numInputs,
                                 sl::CommandBuffer* cmdBuffer)
    {
        return Nvidia::g_slEvaluateFeature ? Nvidia::g_slEvaluateFeature(feature, frame, inputs, numInputs, cmdBuffer)
                                           : sl::Result::eErrorNotInitialized;
    }
    sl::Result slIsFeatureSupported(sl::Feature feature, const sl::AdapterInfo& adapterInfo)
    {
        return Nvidia::g_slIsFeatureSupported ? Nvidia::g_slIsFeatureSupported(feature, adapterInfo)
                                              : sl::Result::eErrorNotInitialized;
    }
    sl::Result slAllocateResources(sl::CommandBuffer* cmdBuffer,
                                   sl::Feature feature,
                                   const sl::ViewportHandle& viewport)
    {
        return Nvidia::g_slAllocateResources ? Nvidia::g_slAllocateResources(cmdBuffer, feature, viewport)
                                             : sl::Result::eErrorNotInitialized;
    }
    sl::Result slFreeResources(sl::Feature feature, const sl::ViewportHandle& viewport)
    {
        return Nvidia::g_slFreeResources ? Nvidia::g_slFreeResources(feature, viewport)
                                         : sl::Result::eErrorNotInitialized;
    }
}
