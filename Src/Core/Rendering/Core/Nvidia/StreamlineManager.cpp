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
static PFun_slAllocateResources* g_slAllocateResources = nullptr;

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
    HMODULE hModule = LoadLibraryA("sl.interposer.dll");
    if (!hModule)
    {
        hModule = LoadLibraryA("External/additional_libs/NVStreamline/sl.interposer.dll");
    }

    if (!hModule)
    {
        std::cout << "[StreamlineManager] Failed to load sl.interposer.dll" << std::endl;
        return false;
    }

    g_slInit = (PFun_slInit*)GetProcAddress(hModule, "slInit");
    g_slShutdown = (PFun_slShutdown*)GetProcAddress(hModule, "slShutdown");
    g_slGetFeatureFunction = (PFun_slGetFeatureFunction*)GetProcAddress(hModule, "slGetFeatureFunction");
    g_slGetNewFrameToken = (PFun_slGetNewFrameToken*)GetProcAddress(hModule, "slGetNewFrameToken");
    g_slSetVulkanInfo = (PFun_slSetVulkanInfo*)GetProcAddress(hModule, "slSetVulkanInfo");
    g_slSetTagForFrame = (PFun_slSetTagForFrame*)GetProcAddress(hModule, "slSetTagForFrame");
    g_slSetConstants = (PFun_slSetConstants*)GetProcAddress(hModule, "slSetConstants");
    g_slEvaluateFeature = (PFun_slEvaluateFeature*)GetProcAddress(hModule, "slEvaluateFeature");
    g_slIsFeatureSupported = (PFun_slIsFeatureSupported*)GetProcAddress(hModule, "slIsFeatureSupported");
    g_slAllocateResources = (PFun_slAllocateResources*)GetProcAddress(hModule, "slAllocateResources");

    if (!g_slInit || !g_slShutdown || !g_slGetFeatureFunction || !g_slGetNewFrameToken || 
        !g_slSetVulkanInfo || !g_slSetTagForFrame || !g_slSetConstants || !g_slEvaluateFeature ||
        !g_slIsFeatureSupported || !g_slAllocateResources)
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
    if (!g_slSetVulkanInfo) return false;

    const auto& context = VkGlobals::GetContext();
    const auto& indices = VkGlobals::GetQueueFamilyIndices();
    
    sl::VulkanInfo info{};
    info.instance = context.Instance;
    info.device = context.Device;
    info.physicalDevice = context.PhysicalDevice;
    info.graphicsQueueFamily = indices.graphicsFamily.value_or(0);
    info.computeQueueFamily = indices.computeFamily.value_or(0);
    info.graphicsQueueIndex = 0;
    info.computeQueueIndex = 0;

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

    s_initialized = true;
    return true;
}

void StreamlineManager::Shutdown()
{
    if (s_initialized && g_slShutdown)
    {
        g_slShutdown();
        s_initialized = false;
    }
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
    return slDLSSSetOptions(viewport, options) == sl::Result::eOk;
}

void StreamlineManager::EvaluateDLSS(VkCommandBuffer cmdBuf, u32 frameIdx)
{
    if (!s_initialized || !g_slGetNewFrameToken || !g_slEvaluateFeature) return;

    sl::FrameToken* frameToken = nullptr;
    g_slGetNewFrameToken(frameToken, &frameIdx);

    sl::ViewportHandle viewport(0);
    const sl::BaseStructure* inputs[] = {&viewport};
    g_slEvaluateFeature(sl::kFeatureDLSS, *frameToken, inputs, 1, (sl::CommandBuffer*)cmdBuf);
}

void StreamlineManager::AllocateResources(VkCommandBuffer cmdBuf, sl::Feature feature)
{
    if (!s_initialized || !g_slAllocateResources) return;
    sl::ViewportHandle viewport(0);
    g_slAllocateResources((sl::CommandBuffer*)cmdBuf, feature, viewport);
}

} // namespace Nvidia

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
}
