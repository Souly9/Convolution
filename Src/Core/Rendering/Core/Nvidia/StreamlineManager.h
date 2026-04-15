#pragma once
#include "Core/Global/GlobalDefines.h"
#include <vulkan/vulkan.h>
#include "sl.h"
#include "sl_dlss.h"

namespace Nvidia
{
class StreamlineManager
{
public:
    static bool EarlyInit();
    static bool Init();
    static void Shutdown();

    static bool SetDLSSOptions(u32 width, u32 height, sl::DLSSMode mode);
    static void EvaluateDLSS(VkCommandBuffer cmdBuf, u32 frameIdx);
    static void AllocateResources(VkCommandBuffer cmdBuf, sl::Feature feature);

    static bool IsAvailable() { return s_initialized; }

private:
    static bool s_initialized;
};
} // namespace Nvidia
