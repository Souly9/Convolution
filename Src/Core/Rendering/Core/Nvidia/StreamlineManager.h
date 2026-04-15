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

    static bool GetFrameToken(u32 frameIdx, sl::FrameToken*& pFrameToken);
    static bool GetDLSSFeatureRequirements(sl::FeatureRequirements& requirements);
    static void SetVulkanQueueStartIndices(u32 graphicsQueueIndex, u32 computeQueueIndex);
    static bool EnsureDLSSConfigured(u32 width, u32 height, sl::DLSSMode mode);
    static bool ConsumeDLSSResetFlag();
    static bool SetDLSSOptions(u32 width, u32 height, sl::DLSSMode mode);
    static bool FreeResources(sl::Feature feature);
    static bool EvaluateDLSS(VkCommandBuffer cmdBuf, const sl::FrameToken& frameToken);
    static bool AllocateResources(VkCommandBuffer cmdBuf, sl::Feature feature);
    static bool IsDLSSEvaluateBlocked();

    static bool IsAvailable() { return s_initialized; }

private:
    static bool s_initialized;
};
} // namespace Nvidia
