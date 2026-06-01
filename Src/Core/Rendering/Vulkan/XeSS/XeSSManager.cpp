#include "XeSSManager.h"
#include "Core/Global/LogDefines.h"

namespace VulkanXeSS
{
namespace
{
const char* XeSSResultStr(xess_result_t r)
{
    switch (r)
    {
        case XESS_RESULT_SUCCESS:                    return "SUCCESS";
        case XESS_RESULT_ERROR_UNSUPPORTED_DEVICE:   return "UNSUPPORTED_DEVICE";
        case XESS_RESULT_ERROR_UNSUPPORTED_DRIVER:   return "UNSUPPORTED_DRIVER";
        case XESS_RESULT_ERROR_UNINITIALIZED:        return "UNINITIALIZED";
        case XESS_RESULT_ERROR_INVALID_ARGUMENT:     return "INVALID_ARGUMENT";
        case XESS_RESULT_ERROR_DEVICE_OUT_OF_MEMORY: return "DEVICE_OUT_OF_MEMORY";
        case XESS_RESULT_ERROR_DEVICE:               return "DEVICE_ERROR";
        case XESS_RESULT_ERROR_NOT_IMPLEMENTED:      return "NOT_IMPLEMENTED";
        case XESS_RESULT_ERROR_INVALID_CONTEXT:      return "INVALID_CONTEXT";
        case XESS_RESULT_ERROR_OPERATION_IN_PROGRESS:return "OPERATION_IN_PROGRESS";
        case XESS_RESULT_ERROR_UNSUPPORTED:          return "UNSUPPORTED";
        case XESS_RESULT_ERROR_CANT_LOAD_LIBRARY:    return "CANT_LOAD_LIBRARY";
        case XESS_RESULT_ERROR_WRONG_CALL_ORDER:     return "WRONG_CALL_ORDER";
        case XESS_RESULT_ERROR_UNKNOWN:              return "UNKNOWN_INTERNAL";
        default:                                     return "UNKNOWN";
    }
}

void XeSSLogCallback(const char* message, xess_logging_level_t level)
{
    switch (level)
    {
        case XESS_LOGGING_LEVEL_ERROR:   DEBUG_LOG_ERRF("[XeSS] {}", message); break;
        case XESS_LOGGING_LEVEL_WARNING: DEBUG_LOG_WARNF("[XeSS] {}", message); break;
        default:                         DEBUG_LOGF("[XeSS] {}", message);       break;
    }
}
}

bool XeSSManager::Initialize()
{
    if (s_initialized)
        return true;

    s_xessDllLoaded = true;
    s_initialized = true;
    DEBUG_LOG("[XeSSManager] Intel XeSS initialized successfully.");
    return true;
}

void XeSSManager::Shutdown()
{
    if (s_context)
    {
        xessDestroyContext(s_context);
        s_context = nullptr;
    }
    s_currentWidth = 0;
    s_currentHeight = 0;
    s_currentQuality = static_cast<xess_quality_settings_t>(0);
}

bool XeSSManager::IsSupported()
{
    if (!s_initialized)
        Initialize();
    return s_xessDllLoaded;
}

bool XeSSManager::EnsureConfigured(VkInstance instance,
                                   VkPhysicalDevice physicalDevice,
                                   VkDevice device,
                                   u32 outputWidth,
                                   u32 outputHeight,
                                   xess_quality_settings_t qualitySetting)
{
    if (!s_initialized && !Initialize())
        return false;

    if (s_context && s_currentWidth == outputWidth && s_currentHeight == outputHeight && s_currentQuality == qualitySetting)
        return true;

    Shutdown();

    DEBUG_LOGF("[XeSSManager] Creating context: Output={}x{}, Quality={}", outputWidth, outputHeight, static_cast<int>(qualitySetting));

    xess_result_t res = xessVKCreateContext(instance, physicalDevice, device, &s_context);
    if (res != XESS_RESULT_SUCCESS)
    {
        DEBUG_LOG_ERRF("[XeSSManager] xessVKCreateContext failed: {} ({})", XeSSResultStr(res), static_cast<int>(res));
        s_context = nullptr;
        return false;
    }
    DEBUG_LOG("[XeSSManager] xessVKCreateContext succeeded.");

    xessSetLoggingCallback(s_context, XESS_LOGGING_LEVEL_DEBUG, XeSSLogCallback);

    xess_properties_t props{};
    xess_2d_t outputRes{outputWidth, outputHeight};
    if (xessGetProperties(s_context, &outputRes, &props) == XESS_RESULT_SUCCESS)
    {
        DEBUG_LOGF("[XeSSManager] XeSS properties: tempBufferHeapSize={}, tempTextureHeapSize={}, descriptors={}",
                   props.tempBufferHeapSize, props.tempTextureHeapSize, props.requiredDescriptorCount);
    }

    xess_vk_init_params_t initParams{};
    initParams.outputResolution.x = outputWidth;
    initParams.outputResolution.y = outputHeight;
    initParams.qualitySetting = qualitySetting;
    initParams.initFlags = XESS_INIT_FLAG_USE_NDC_VELOCITY | XESS_INIT_FLAG_INVERTED_DEPTH | XESS_INIT_FLAG_ENABLE_AUTOEXPOSURE;

    DEBUG_LOGF("[XeSSManager] Calling xessVKInit with flags=0x{:08X}", static_cast<u32>(initParams.initFlags));
    res = xessVKInit(s_context, &initParams);
    if (res != XESS_RESULT_SUCCESS)
    {
        DEBUG_LOG_ERRF("[XeSSManager] xessVKInit failed: {} ({})", XeSSResultStr(res), static_cast<int>(res));
        xessDestroyContext(s_context);
        s_context = nullptr;
        return false;
    }
    DEBUG_LOG("[XeSSManager] xessVKInit succeeded.");

    res = xessSetVelocityScale(s_context, -1.0f, -1.0f);
    if (res != XESS_RESULT_SUCCESS)
        DEBUG_LOG_WARNF("[XeSSManager] xessSetVelocityScale failed: {} ({})", XeSSResultStr(res), static_cast<int>(res));

    res = xessSetMaxResponsiveMaskValue(s_context, 0.05f);
    if (res != XESS_RESULT_SUCCESS)
        DEBUG_LOG_WARNF("[XeSSManager] xessSetMaxResponsiveMaskValue failed: {} ({})", XeSSResultStr(res), static_cast<int>(res));

    s_currentWidth = outputWidth;
    s_currentHeight = outputHeight;
    s_currentQuality = qualitySetting;
    s_needsReset = true;

    DEBUG_LOGF("[XeSSManager] Intel XeSS configured: Output={}x{}, Quality={}", outputWidth, outputHeight, static_cast<int>(qualitySetting));
    return true;
}

xess_context_handle_t XeSSManager::GetContext()
{
    return s_context;
}

bool XeSSManager::GetOptimalResolution(u32 outputWidth,
                                       u32 outputHeight,
                                       xess_quality_settings_t qualitySetting,
                                       u32& outRenderWidth,
                                       u32& outRenderHeight)
{
    if (!s_context)
    {
        float scale = 1.0f;
        switch (qualitySetting)
        {
            case XESS_QUALITY_SETTING_ULTRA_PERFORMANCE:  scale = 0.50f; break;
            case XESS_QUALITY_SETTING_PERFORMANCE:        scale = 0.59f; break;
            case XESS_QUALITY_SETTING_BALANCED:           scale = 0.67f; break;
            case XESS_QUALITY_SETTING_QUALITY:            scale = 0.77f; break;
            case XESS_QUALITY_SETTING_ULTRA_QUALITY:      scale = 0.83f; break;
            case XESS_QUALITY_SETTING_ULTRA_QUALITY_PLUS: scale = 0.91f; break;
            default: scale = 1.0f; break;
        }
        outRenderWidth  = static_cast<u32>(outputWidth  * scale + 0.5f);
        outRenderHeight = static_cast<u32>(outputHeight * scale + 0.5f);
        return true;
    }

    xess_2d_t outRes{};
    xess_2d_t outResolution{outputWidth, outputHeight};
    xess_result_t res = xessGetInputResolution(s_context, &outResolution, qualitySetting, &outRes);
    if (res == XESS_RESULT_SUCCESS)
    {
        outRenderWidth  = outRes.x;
        outRenderHeight = outRes.y;
        return true;
    }
    return false;
}

bool XeSSManager::Execute(VkCommandBuffer cmdBuf, const xess_vk_execute_params_t& execParams)
{
    if (!s_context)
    {
        DEBUG_LOG_ERR("[XeSSManager] Execute called with null context.");
        return false;
    }

    DEBUG_LOGF("[XeSSManager] Execute: input={}x{}, reset={}, jitter=({:.4f},{:.4f})",
               execParams.inputWidth, execParams.inputHeight,
               execParams.resetHistory,
               execParams.jitterOffsetX, execParams.jitterOffsetY);
    DEBUG_LOGF("[XeSSManager]   color  image=0x{:016X} view=0x{:016X} fmt={} {}x{}",
               reinterpret_cast<uintptr_t>(execParams.colorTexture.image),
               reinterpret_cast<uintptr_t>(execParams.colorTexture.imageView),
               static_cast<int>(execParams.colorTexture.format),
               execParams.colorTexture.width, execParams.colorTexture.height);
    DEBUG_LOGF("[XeSSManager]   depth  image=0x{:016X} view=0x{:016X} fmt={} {}x{}",
               reinterpret_cast<uintptr_t>(execParams.depthTexture.image),
               reinterpret_cast<uintptr_t>(execParams.depthTexture.imageView),
               static_cast<int>(execParams.depthTexture.format),
               execParams.depthTexture.width, execParams.depthTexture.height);
    DEBUG_LOGF("[XeSSManager]   motion image=0x{:016X} view=0x{:016X} fmt={} {}x{}",
               reinterpret_cast<uintptr_t>(execParams.velocityTexture.image),
               reinterpret_cast<uintptr_t>(execParams.velocityTexture.imageView),
               static_cast<int>(execParams.velocityTexture.format),
               execParams.velocityTexture.width, execParams.velocityTexture.height);
    DEBUG_LOGF("[XeSSManager]   output image=0x{:016X} view=0x{:016X} fmt={} {}x{}",
               reinterpret_cast<uintptr_t>(execParams.outputTexture.image),
               reinterpret_cast<uintptr_t>(execParams.outputTexture.imageView),
               static_cast<int>(execParams.outputTexture.format),
               execParams.outputTexture.width, execParams.outputTexture.height);

    xess_result_t res = xessVKExecute(s_context, cmdBuf, &execParams);
    if (res != XESS_RESULT_SUCCESS)
    {
        DEBUG_LOG_ERRF("[XeSSManager] xessVKExecute failed: {} ({})", XeSSResultStr(res), static_cast<int>(res));
        return false;
    }
    return true;
}

void XeSSManager::ResetHistory()
{
    s_needsReset = true;
}

bool XeSSManager::ConsumeResetFlag()
{
    const bool needsReset = s_needsReset;
    s_needsReset = false;
    return needsReset;
}
} // namespace VulkanXeSS
