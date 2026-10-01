#pragma once
#include "Core/Global/GlobalDefines.h"

struct RendererState;
namespace RenderPasses
{
class PassManager;
}

// Vulkan-only vendor SDK hooks, forwarded from RenderBackendImpl<Vulkan>
namespace VkVendor
{
bool IsDLSSSupported();
bool IsDLSSRRSupported();
bool IsXeSSSupported();
bool IsDLSSDebugUIAvailable();
void AddUpscalerPasses(RenderPasses::PassManager& passManager);
void BeginFrame(u32 frameIdx);
void DrawSettingsUI();
void DrawDiagnosticsUI(const RendererState& state);
} // namespace VkVendor
