#pragma once
#include "GlobalDefines.h"
#include "Core/Rendering/Core/RenderDefinitions.h"

extern u32 g_currentFrameNumber;
extern mathstl::Vector2 g_swapChainExtent;
extern TexFormat g_swapChainFormat;

namespace FrameGlobals
{
static inline u32 GetFrameNumber()
{
    return g_currentFrameNumber;
}
static inline void SetFrameNumber(u32 frameNumber)
{
    g_currentFrameNumber = frameNumber;
}
static inline u32 GetPreviousFrameNumber(u32 frameNumber)
{
    return (frameNumber == 0) ? (SWAPCHAIN_IMAGES - 1) : (frameNumber - 1);
}

static inline const mathstl::Vector2& GetSwapChainExtent()
{
    return g_swapChainExtent;
}
static inline f32 GetScreenAspectRatio()
{
    return static_cast<f32>(g_swapChainExtent.x) / static_cast<f32>(g_swapChainExtent.y);
}
static inline void SetSwapChainExtent(const mathstl::Vector2& swapChainExtent)
{
    g_swapChainExtent = swapChainExtent;
}
static inline TexFormat GetSwapChainFormat()
{
    return g_swapChainFormat;
}
static inline void SetSwapChainFormat(TexFormat format)
{
    g_swapChainFormat = format;
}
} // namespace FrameGlobals
