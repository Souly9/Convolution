#include "StreamlineManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Global/ThreadBase.h"
#include "Core/Rendering/Vulkan/VkBackendAccess.h"
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <string>
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <vulkan/vulkan.h>
#include <windows.h>

#if CONV_WITH_STREAMLINE
#include <sl_helpers_vk.h>

// Set by CMake when the development Streamline/NGX DLLs are deployed (they provide the debug overlays)
#ifndef CONV_SL_DEVELOPMENT_DLLS
#define CONV_SL_DEVELOPMENT_DLLS 0
#endif

namespace Nvidia
{
bool StreamlineManager::s_initialized = false;

namespace
{
struct FeatureState
{
    bool configured{false};
    u32 width{0};
    u32 height{0};
    sl::DLSSMode mode{sl::DLSSMode::eOff};
    u32 optionsKey{0};
    bool lastConfigureFailed{false};
    bool evaluateBlocked{false};
    bool needsReset{false};
};

bool g_slInitialized = false;
bool g_dlssSupported = false;
bool g_dlssRRSupported = false;
bool g_imguiPluginRequested = false;
bool g_imguiPluginAvailable = false;
FeatureState g_features[static_cast<u32>(DLSSVariant::Count)]{};
StreamlineManager::SwapchainFunctions g_swapchainFunctions{};
bool g_presentRouted = false;
StreamlineManager::VersionInfo g_versionInfo{};

ProfiledLockable(CustomMutex, g_debugMutex);
StreamlineManager::DLSSDebugState g_dlssDebugState{};
StreamlineManager::DebugSettings g_debugSettings{};
// Read from Streamline's logging threads, so kept outside the mutex
std::atomic<u8> g_logVerbosity{static_cast<u8>(StreamlineManager::LogVerbosity::Warnings)};

u32 g_slGraphicsQueueStartIndex = 0;
u32 g_slComputeQueueStartIndex = 0;
sl::FrameToken* s_frameTokens[FRAMES_IN_FLIGHT] = {nullptr};

FeatureState& GetState(DLSSVariant variant)
{
    return g_features[static_cast<u32>(variant)];
}

sl::Feature ToFeature(DLSSVariant variant)
{
    return variant == DLSSVariant::RayReconstruction ? sl::kFeatureDLSS_RR : sl::kFeatureDLSS;
}

bool IsRenderDocLoaded()
{
    return GetModuleHandleA("renderdoc.dll") != nullptr;
}

bool DirectoryExists(const wchar_t* path)
{
    const DWORD attrs = GetFileAttributesW(path);
    return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

bool IsEnvFlagSet(const char* name)
{
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0' && value[0] != '0';
}

std::wstring GetExecutableDirectory()
{
    wchar_t buffer[MAX_PATH]{};
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    std::wstring path(buffer);
    const size_t lastSlash = path.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos)
    {
        return path.substr(0, lastSlash);
    }
    return L".";
}

std::wstring GetAbsolutePath(const wchar_t* path)
{
    wchar_t buffer[MAX_PATH]{};
    const DWORD len = GetFullPathNameW(path, MAX_PATH, buffer, nullptr);
    return len > 0 && len < MAX_PATH ? std::wstring(buffer, len) : std::wstring(path);
}

void SL_LoggingCallback(sl::LogType type, const char* msg)
{
    const auto verbosity = static_cast<StreamlineManager::LogVerbosity>(g_logVerbosity.load(std::memory_order_relaxed));
    if (type == sl::LogType::eError)
    {
        if (verbosity >= StreamlineManager::LogVerbosity::Errors)
            DEBUG_LOG_ERRF("[Streamline] [ERROR] {}", msg);
    }
    else if (type == sl::LogType::eWarn)
    {
        if (verbosity >= StreamlineManager::LogVerbosity::Warnings)
            DEBUG_LOG_WARNF("[Streamline] [WARN] {}", msg);
    }
    else if (verbosity >= StreamlineManager::LogVerbosity::Info)
    {
        DEBUG_LOGF("[Streamline] [INFO] {}", msg);
    }
}

// Options that force a reconfigure (and history reset) when changed from the UI
u32 MakeOptionsKey(DLSSVariant variant, const StreamlineManager::DebugSettings& settings)
{
    u32 key = settings.invertIndicatorAxisX ? 1u : 0u;
    key |= settings.invertIndicatorAxisY ? 2u : 0u;
    if (variant == DLSSVariant::SuperResolution)
    {
        key |= settings.useExposureTexture ? 4u : 0u;
        key |= static_cast<u32>(settings.preset) << 8;
    }
    return key;
}

bool SetSuperResolutionOptions(u32 width, u32 height, sl::DLSSMode mode, const StreamlineManager::DebugSettings& settings)
{
    sl::DLSSOptions options{};
    options.mode = mode;
    options.outputWidth = width;
    options.outputHeight = height;
    options.colorBuffersHDR = sl::Boolean::eTrue;
    options.useAutoExposure = settings.useExposureTexture ? sl::Boolean::eFalse : sl::Boolean::eTrue;
    options.indicatorInvertAxisX = settings.invertIndicatorAxisX ? sl::Boolean::eTrue : sl::Boolean::eFalse;
    options.indicatorInvertAxisY = settings.invertIndicatorAxisY ? sl::Boolean::eTrue : sl::Boolean::eFalse;
    options.dlaaPreset = settings.preset;
    options.qualityPreset = settings.preset;
    options.balancedPreset = settings.preset;
    options.performancePreset = settings.preset;
    options.ultraPerformancePreset = settings.preset;
    options.ultraQualityPreset = settings.preset;

    const sl::Result res = slDLSSSetOptions(sl::ViewportHandle(0), options);
    if (res != sl::Result::eOk)
        DEBUG_LOG_WARNF("[StreamlineManager] slDLSSSetOptions failed with result: 0x{:X}", static_cast<u32>(res));
    return res == sl::Result::eOk;
}

bool SetRayReconstructionOptions(u32 width,
                                 u32 height,
                                 sl::DLSSMode mode,
                                 const mathstl::Matrix& worldToView,
                                 const mathstl::Matrix& viewToWorld,
                                 const StreamlineManager::DebugSettings& settings)
{
    sl::DLSSDOptions options{};
    options.mode = mode;
    options.outputWidth = width;
    options.outputHeight = height;
    options.colorBuffersHDR = sl::Boolean::eTrue;
    options.normalRoughnessMode = sl::DLSSDNormalRoughnessMode::eUnpacked;
    options.indicatorInvertAxisX = settings.invertIndicatorAxisX ? sl::Boolean::eTrue : sl::Boolean::eFalse;
    options.indicatorInvertAxisY = settings.invertIndicatorAxisY ? sl::Boolean::eTrue : sl::Boolean::eFalse;
    std::memcpy(&options.worldToCameraView, &worldToView, sizeof(options.worldToCameraView));
    std::memcpy(&options.cameraViewToWorld, &viewToWorld, sizeof(options.cameraViewToWorld));

    const sl::Result res = slDLSSDSetOptions(sl::ViewportHandle(0), options);
    if (res != sl::Result::eOk)
        DEBUG_LOG_WARNF("[StreamlineManager] slDLSSDSetOptions failed with result: 0x{:X}", static_cast<u32>(res));
    return res == sl::Result::eOk;
}

void PublishConfigState(DLSSVariant variant, const FeatureState& state, u32 width, u32 height, sl::DLSSMode mode)
{
    auto debugState = StreamlineManager::GetDLSSDebugState();
    debugState.variant = variant;
    debugState.configured = state.configured;
    debugState.lastConfigureFailed = state.lastConfigureFailed;
    debugState.evaluateBlocked = state.evaluateBlocked;
    debugState.configuredMode = mode;
    debugState.outputWidth = width;
    debugState.outputHeight = height;

    if (state.configured && variant == DLSSVariant::SuperResolution)
    {
        sl::DLSSOptions options{};
        options.mode = mode;
        options.outputWidth = width;
        options.outputHeight = height;
        sl::DLSSOptimalSettings optimal{};
        if (slDLSSGetOptimalSettings(options, optimal) == sl::Result::eOk)
        {
            debugState.optimalRenderWidth = optimal.optimalRenderWidth;
            debugState.optimalRenderHeight = optimal.optimalRenderHeight;
            debugState.renderWidthMin = optimal.renderWidthMin;
            debugState.renderHeightMin = optimal.renderHeightMin;
            debugState.renderWidthMax = optimal.renderWidthMax;
            debugState.renderHeightMax = optimal.renderHeightMax;
        }
    }
    StreamlineManager::SetDLSSDebugState(debugState);
}

// Streamline requires the host to route these hooks when manual hooking is used; opt-in until verified
void RouteSwapchainThroughStreamline()
{
    HMODULE interposer = GetModuleHandleW(L"sl.interposer.dll");
    if (interposer == nullptr)
    {
        DEBUG_LOG_WARN("[StreamlineManager] CONV_SL_OVERLAY set but sl.interposer.dll is not loaded");
        return;
    }

    StreamlineManager::SwapchainFunctions routed{};
    routed.createSwapchain = reinterpret_cast<PFN_vkCreateSwapchainKHR>(GetProcAddress(interposer, "vkCreateSwapchainKHR"));
    routed.destroySwapchain = reinterpret_cast<PFN_vkDestroySwapchainKHR>(GetProcAddress(interposer, "vkDestroySwapchainKHR"));
    routed.getSwapchainImages = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(GetProcAddress(interposer, "vkGetSwapchainImagesKHR"));
    routed.acquireNextImage = reinterpret_cast<PFN_vkAcquireNextImageKHR>(GetProcAddress(interposer, "vkAcquireNextImageKHR"));
    routed.queuePresent = reinterpret_cast<PFN_vkQueuePresentKHR>(GetProcAddress(interposer, "vkQueuePresentKHR"));
    routed.deviceWaitIdle = reinterpret_cast<PFN_vkDeviceWaitIdle>(GetProcAddress(interposer, "vkDeviceWaitIdle"));

    if (!routed.createSwapchain || !routed.destroySwapchain || !routed.getSwapchainImages || !routed.acquireNextImage ||
        !routed.queuePresent || !routed.deviceWaitIdle)
    {
        DEBUG_LOG_WARN("[StreamlineManager] sl.interposer.dll is missing swapchain exports, present stays native");
        return;
    }

    g_swapchainFunctions = routed;
    g_presentRouted = true;
    DEBUG_LOG("[StreamlineManager] Swapchain and present are routed through Streamline");
}

void QueryVersionInfo()
{
    StreamlineManager::VersionInfo info{};
    info.sdk = sl::Version(SL_VERSION_MAJOR, SL_VERSION_MINOR, SL_VERSION_PATCH);
    info.developmentPlugins = CONV_SL_DEVELOPMENT_DLLS != 0;

    sl::FeatureVersion version{};
    if (slGetFeatureVersion(sl::kFeatureDLSS, version) == sl::Result::eOk)
    {
        info.dlssPlugin = version.versionSL;
        info.dlssNGX = version.versionNGX;
    }
    if (g_dlssRRSupported && slGetFeatureVersion(sl::kFeatureDLSS_RR, version) == sl::Result::eOk)
    {
        info.rrPlugin = version.versionSL;
        info.rrNGX = version.versionNGX;
    }

    sl::FeatureRequirements requirements{};
    if (slGetFeatureRequirements(sl::kFeatureDLSS, requirements) == sl::Result::eOk)
        info.dlssFlags = requirements.flags;

    SimpleScopedGuard lock(g_debugMutex);
    g_versionInfo = info;
}
} // namespace

bool StreamlineManager::EarlyInit()
{
    if (g_slInitialized)
        return true;

    if (IsRenderDocLoaded())
    {
        DEBUG_LOG_WARN("[StreamlineManager] RenderDoc detected. DLSS/Streamline disabled for this run.");
        return false;
    }

    const std::wstring exeDir = GetExecutableDirectory();
    const std::wstring extDir = exeDir + L"/../../External/additional_libs/NVStreamline";
    const std::wstring resolvedExtDir = GetAbsolutePath(extDir.c_str());

    // Streamline's overlays come from sl.imgui and only exist in the development DLLs
    g_imguiPluginRequested = CONV_SL_DEVELOPMENT_DLLS != 0 && IsEnvFlagSet("CONV_SL_OVERLAY");

    sl::Preferences pref{};
    sl::Feature features[] = {sl::kFeatureDLSS, sl::kFeatureDLSS_RR, sl::kFeatureImGUI};
    pref.featuresToLoad = features;
    pref.numFeaturesToLoad = g_imguiPluginRequested ? 3u : 2u;
    pref.renderAPI = sl::RenderAPI::eVulkan;
    pref.flags |= sl::PreferenceFlags::eUseFrameBasedResourceTagging | sl::PreferenceFlags::eUseManualHooking;
    pref.logMessageCallback = SL_LoggingCallback;
    // Filtered at runtime by the UI verbosity
    pref.logLevel = sl::LogLevel::eDefault;

    // sl.imgui is not deployed next to the exe, the opt-in overlay loads it from the development folder
    const std::wstring developmentDir = resolvedExtDir + L"/development";
    const wchar_t* pluginPaths[3] = {exeDir.c_str()};
    u32 numPluginPaths = 1;
    if (g_imguiPluginRequested && DirectoryExists(developmentDir.c_str()))
        pluginPaths[numPluginPaths++] = developmentDir.c_str();
    if (DirectoryExists(resolvedExtDir.c_str()))
        pluginPaths[numPluginPaths++] = resolvedExtDir.c_str();
    pref.pathsToPlugins = pluginPaths;
    pref.numPathsToPlugins = numPluginPaths;

    pref.engine = sl::EngineType::eCustom;
    pref.engineVersion = "1.0.0";
    pref.projectId = "2f0f5f9e-13d4-4d63-bb9b-06f7f8cf1d5f";

    sl::Result res = slInit(pref, sl::kSDKVersion);
    if (res != sl::Result::eOk)
    {
        DEBUG_LOG_WARNF("[StreamlineManager] slInit failed with error: {}", static_cast<int>(res));
        return false;
    }

    g_slInitialized = true;
    return true;
}

bool StreamlineManager::IsEarlyInitialized()
{
    return g_slInitialized;
}

bool StreamlineManager::Init()
{
    if (s_initialized)
        return true;
    if (!g_slInitialized)
        return false;

    const auto queueFamilies = VkBackend::QueueFamilies();

    sl::VulkanInfo vulkanInfo{};
    vulkanInfo.instance = VkBackend::Instance();
    vulkanInfo.device = VkBackend::Device();
    vulkanInfo.physicalDevice = VkBackend::PhysicalDevice();
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

    sl::AdapterInfo adapterInfo{};
    adapterInfo.vkPhysicalDevice = VkBackend::PhysicalDevice();

    VkPhysicalDeviceIDProperties physicalDeviceIDProperties{};
    physicalDeviceIDProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;

    VkPhysicalDeviceProperties2 physicalDeviceProperties2{};
    physicalDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    physicalDeviceProperties2.pNext = &physicalDeviceIDProperties;

    vkGetPhysicalDeviceProperties2(VkBackend::PhysicalDevice(), &physicalDeviceProperties2);

    if (physicalDeviceIDProperties.deviceLUIDValid)
    {
        adapterInfo.deviceLUID = physicalDeviceIDProperties.deviceLUID;
        adapterInfo.deviceLUIDSizeInBytes = VK_LUID_SIZE;
    }

    res = slIsFeatureSupported(sl::kFeatureDLSS, adapterInfo);
    g_dlssSupported = res == sl::Result::eOk;
    if (res != sl::Result::eOk)
    {
        DEBUG_LOG_WARNF("[StreamlineManager] DLSS feature unsupported on selected adapter: {}", static_cast<int>(res));
        Shutdown();
        return false;
    }

    res = slIsFeatureSupported(sl::kFeatureDLSS_RR, adapterInfo);
    g_dlssRRSupported = res == sl::Result::eOk;
    if (g_dlssRRSupported)
        DEBUG_LOG("[StreamlineManager] DLSS Ray Reconstruction (DLSS-RR) is supported on this adapter.");
    else
        DEBUG_LOG_WARNF("[StreamlineManager] DLSS-RR is not supported on this adapter: error = {}", static_cast<int>(res));

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

    if (g_imguiPluginRequested)
    {
        bool loaded = false;
        g_imguiPluginAvailable = slIsFeatureLoaded(sl::kFeatureImGUI, loaded) == sl::Result::eOk && loaded;
        if (g_imguiPluginAvailable)
            RouteSwapchainThroughStreamline();
        else
            DEBUG_LOG_WARN("[StreamlineManager] Streamline ImGui plugin did not load, overlays unavailable");
    }

    s_initialized = true;
    QueryVersionInfo();

    auto debugState = GetDLSSDebugState();
    debugState.streamlineInitialized = true;
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
    g_dlssSupported = false;
    g_dlssRRSupported = false;
    g_imguiPluginRequested = false;
    g_imguiPluginAvailable = false;
    for (auto& state : g_features)
        state = FeatureState{};
    g_swapchainFunctions = SwapchainFunctions{};
    g_presentRouted = false;
    g_slGraphicsQueueStartIndex = 0;
    g_slComputeQueueStartIndex = 0;
}

void StreamlineManager::AcquireNewFrameToken(u32 frameIdx)
{
    if (!g_slInitialized)
        return;

    sl::Result res = slGetNewFrameToken(s_frameTokens[frameIdx], nullptr);
    if (res != sl::Result::eOk)
    {
        DEBUG_LOG_ERR("Failed to get SL frame token");
    }
}

bool StreamlineManager::GetFrameToken(u32 frameIdx, sl::FrameToken*& pFrameToken)
{
    if (!g_slInitialized)
        return false;

    pFrameToken = s_frameTokens[frameIdx];
    return pFrameToken != nullptr;
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

bool StreamlineManager::GetDLSSRRFeatureRequirements(sl::FeatureRequirements& requirements)
{
    if (!g_slInitialized)
        return false;
    const sl::Result res = slGetFeatureRequirements(sl::kFeatureDLSS_RR, requirements);
    if (res != sl::Result::eOk)
    {
        DEBUG_LOG_WARNF("[StreamlineManager] slGetFeatureRequirements(DLSS_RR) failed with result: 0x{:X}",
                        static_cast<u32>(res));
        return false;
    }
    return true;
}

void StreamlineManager::SetVulkanQueueStartIndices(u32 graphicsQueueIndex, u32 computeQueueIndex)
{
    g_slGraphicsQueueStartIndex = graphicsQueueIndex;
    g_slComputeQueueStartIndex = computeQueueIndex;
}

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
    if (!s_initialized || outputWidth == 0 || outputHeight == 0)
        return false;

    FeatureState& state = GetState(variant);
    const DebugSettings settings = GetDebugSettings();
    const u32 optionsKey = MakeOptionsKey(variant, settings);
    // The stored config is the last one attempted, whether it succeeded or not
    const bool sameConfig = state.width == outputWidth && state.height == outputHeight && state.mode == mode &&
                            state.optionsKey == optionsKey;
    const bool configChanged = !state.configured || !sameConfig;
    const bool isRR = variant == DLSSVariant::RayReconstruction;

    // RR carries the camera in its options, so only it re-applies options every frame
    if (!configChanged && !isRR)
        return !state.evaluateBlocked;
    if (state.lastConfigureFailed && sameConfig)
        return false;

    bool ok = false;
    if (isRR)
    {
        // SR options are applied alongside RR for Streamline pipeline compatibility
        SetSuperResolutionOptions(outputWidth, outputHeight, mode, settings);
        ok = SetRayReconstructionOptions(outputWidth, outputHeight, mode, worldToView, viewToWorld, settings);
    }
    else
    {
        ok = SetSuperResolutionOptions(outputWidth, outputHeight, mode, settings);
    }

    state.width = outputWidth;
    state.height = outputHeight;
    state.mode = mode;
    state.optionsKey = optionsKey;
    if (!ok)
    {
        state.configured = false;
        state.lastConfigureFailed = true;
        PublishConfigState(variant, state, outputWidth, outputHeight, mode);
        return false;
    }

    if (configChanged)
    {
        state.configured = true;
        state.lastConfigureFailed = false;
        state.evaluateBlocked = false;
        state.needsReset = true;
        PublishConfigState(variant, state, outputWidth, outputHeight, mode);
    }
    return !state.evaluateBlocked;
}

bool StreamlineManager::ConsumeResetFlag(DLSSVariant variant)
{
    FeatureState& state = GetState(variant);
    const bool needsReset = state.needsReset;
    state.needsReset = false;
    return needsReset;
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

bool StreamlineManager::Evaluate(DLSSVariant variant, VkCommandBuffer cmdBuf, const sl::FrameToken& frameToken)
{
    FeatureState& state = GetState(variant);
    if (!s_initialized || state.evaluateBlocked)
        return false;

    sl::ViewportHandle viewport(0);
    const sl::BaseStructure* inputs[] = {&viewport};
    const auto res = slEvaluateFeature(ToFeature(variant), frameToken, inputs, 1, (sl::CommandBuffer*)cmdBuf);
    if (res != sl::Result::eOk)
    {
        DEBUG_LOG_WARNF("[StreamlineManager] slEvaluateFeature failed with result: 0x{:X}", static_cast<u32>(res));
        if (res == sl::Result::eErrorNGXFailed)
        {
            state.evaluateBlocked = true;
            DEBUG_LOG_WARN("[StreamlineManager] Blocking further evaluate calls for current config to prevent NGX "
                           "recreation/VRAM growth");
        }
    }

    auto debugState = GetDLSSDebugState();
    debugState.lastEvaluateResult = res;
    debugState.evaluateBlocked = state.evaluateBlocked;
    ++debugState.evaluateCallCount;
    if (variant == DLSSVariant::RayReconstruction)
    {
        sl::DLSSDState rrState{};
        if (slDLSSDGetState(viewport, rrState) == sl::Result::eOk)
            debugState.estimatedVRAMUsageInBytes = rrState.estimatedVRAMUsageInBytes;
    }
    else
    {
        sl::DLSSState srState{};
        if (slDLSSGetState(viewport, srState) == sl::Result::eOk)
            debugState.estimatedVRAMUsageInBytes = srState.estimatedVRAMUsageInBytes;
    }
    SetDLSSDebugState(debugState);
    return res == sl::Result::eOk;
}

StreamlineManager::DebugSettings StreamlineManager::GetDebugSettings()
{
    SimpleScopedGuard lock(g_debugMutex);
    return g_debugSettings;
}

void StreamlineManager::SetDebugSettings(const DebugSettings& settings)
{
    SimpleScopedGuard lock(g_debugMutex);
    g_debugSettings = settings;
    g_logVerbosity.store(static_cast<u8>(settings.logVerbosity), std::memory_order_relaxed);
}

StreamlineManager::DLSSDebugState StreamlineManager::GetDLSSDebugState()
{
    SimpleScopedGuard lock(g_debugMutex);
    return g_dlssDebugState;
}

void StreamlineManager::SetDLSSDebugState(const DLSSDebugState& state)
{
    SimpleScopedGuard lock(g_debugMutex);
    g_dlssDebugState = state;
}

StreamlineManager::VersionInfo StreamlineManager::GetVersionInfo()
{
    SimpleScopedGuard lock(g_debugMutex);
    return g_versionInfo;
}

const StreamlineManager::SwapchainFunctions& StreamlineManager::GetSwapchainFunctions()
{
    return g_swapchainFunctions;
}

bool StreamlineManager::IsDLSSSupported()
{
    return s_initialized && g_dlssSupported;
}

bool StreamlineManager::IsDLSSRRSupported()
{
    return s_initialized && g_dlssRRSupported;
}

bool StreamlineManager::IsDLSSDebugUIAvailable()
{
    return s_initialized && g_imguiPluginAvailable && g_presentRouted;
}

} // namespace Nvidia

#endif
