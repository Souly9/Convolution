#include "AftermathManager.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/LogDefines.h"
#include <chrono>

#if CONV_WITH_AFTERMATH_SDK
#include "GFSDK_Aftermath.h"
#include <Windows.h>
#endif

namespace Nvidia
{
#if CONV_WITH_AFTERMATH_SDK
static const char* ToString(GFSDK_Aftermath_Device_Status status)
{
    switch (status)
    {
    case GFSDK_Aftermath_Device_Status_Active: return "Active";
    case GFSDK_Aftermath_Device_Status_Timeout: return "Timeout";
    case GFSDK_Aftermath_Device_Status_OutOfMemory: return "OutOfMemory";
    case GFSDK_Aftermath_Device_Status_PageFault: return "PageFault";
    case GFSDK_Aftermath_Device_Status_Stopped: return "Stopped";
    case GFSDK_Aftermath_Device_Status_Reset: return "Reset";
    case GFSDK_Aftermath_Device_Status_Unknown: return "Unknown";
    case GFSDK_Aftermath_Device_Status_DmaFault: return "DmaFault";
    case GFSDK_Aftermath_Device_Status_DeviceRemovedNoGpuFault: return "DeviceRemovedNoGpuFault";
    default: return "Invalid";
    }
}
#endif

void AftermathManager::LogFenceTimeoutDiagnostics()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

#if CONV_WITH_AFTERMATH_SDK
    HMODULE aftermathModule = ::GetModuleHandleA("GFSDK_Aftermath_Lib.x64.dll");
    if (aftermathModule == nullptr)
    {
        aftermathModule = ::LoadLibraryA("GFSDK_Aftermath_Lib.x64.dll");
    }
    if (aftermathModule == nullptr)
    {
        DEBUG_LOGF("[Aftermath] fence timeout timestamp_ms={} (DLL not loaded)", ms);
        return;
    }

    using FnGetDeviceStatus = GFSDK_Aftermath_Result(GFSDK_AFTERMATH_CALL*)(GFSDK_Aftermath_Device_Status*);
    const auto pGetDeviceStatus =
        reinterpret_cast<FnGetDeviceStatus>(::GetProcAddress(aftermathModule, "GFSDK_Aftermath_GetDeviceStatus"));
    if (pGetDeviceStatus == nullptr)
    {
        DEBUG_LOGF("[Aftermath] fence timeout timestamp_ms={} (GetDeviceStatus unavailable)", ms);
        return;
    }

    GFSDK_Aftermath_Device_Status status = GFSDK_Aftermath_Device_Status_Unknown;
    const GFSDK_Aftermath_Result deviceStatusRslt = pGetDeviceStatus(&status);
    if (GFSDK_Aftermath_SUCCEED(deviceStatusRslt))
    {
        DEBUG_LOGF("[Aftermath] fence timeout timestamp_ms={} device_status={}", ms, ToString(status));
    }
    else
    {
        DEBUG_LOGF("[Aftermath] fence timeout timestamp_ms={} GetDeviceStatus failed result=0x{:X}",
                   ms,
                   static_cast<u32>(deviceStatusRslt));
    }
#else
    DEBUG_LOGF("[Aftermath] fence timeout timestamp_ms={} (SDK disabled)", ms);
#endif
}
} // namespace Nvidia
