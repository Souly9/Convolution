#pragma once
#include "Core/Global/State/States.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/Rendering/Core/RenderGraph/RGResourceID.h"
#include <EASTL/algorithm.h>

// Every AA decision of a frame is resolved here once, all passes read AA::Current()
namespace AA
{
// Temporal technique that actually runs this frame
enum class Temporal : u8
{
    None,
    TAA,
    DLSS,
    DLSSRR,
    XeSS,
};

struct Support
{
    bool dlss{false};
    bool dlssRR{false};
    bool xess{false};
};

struct FrameConfig
{
    Temporal temporal{Temporal::None};
    // Post-tonemap SMAA on the composite output
    bool smaa{false};
    // 0 disables jitter
    u32 jitterPhases{0};
    u32 renderScalePercent{100};
    // HDR color after AA, input of bloom and composite
    RGResourceID aaOutput{RGResourceID::GBufferThisFrameColor};
    // Discard all temporal history this frame
    bool temporalReset{false};

    bool IsUpscaler() const
    {
        return temporal == Temporal::DLSS || temporal == Temporal::DLSSRR || temporal == Temporal::XeSS;
    }
    bool IsDLSS() const { return temporal == Temporal::DLSS || temporal == Temporal::DLSSRR; }
    bool UsesJitter() const { return jitterPhases > 0; }
    RGResourceID CompositeTarget() const { return smaa ? RGResourceID::GBufferPostAAColor : RGResourceID::Swapchain; }
};

inline Temporal ResolveTemporal(const RendererState& state, const Support& support, bool rtReflectionsActive)
{
    switch (state.aaType)
    {
        case AntialiasingType::TAA_SMAA:
            return Temporal::TAA;
        case AntialiasingType::DLSS:
            if (!support.dlss)
                return Temporal::None;
            // RR only replaces SR when it has RT reflections to denoise
            return (support.dlssRR && state.rt.reflectionsUseRayReconstruction && rtReflectionsActive)
                       ? Temporal::DLSSRR
                       : Temporal::DLSS;
        case AntialiasingType::XeSS:
            return support.xess ? Temporal::XeSS : Temporal::None;
        default:
            return Temporal::None;
    }
}

inline bool IsUpscalerMode(AntialiasingType mode)
{
    return mode == AntialiasingType::DLSS || mode == AntialiasingType::XeSS;
}

// Only upscalers may render below output resolution
inline u32 ResolveRenderScalePercent(const RendererState& state, const Support& support)
{
    const bool supported = state.aaType == AntialiasingType::DLSS ? support.dlss : support.xess;
    return IsUpscalerMode(state.aaType) && supported ? eastl::max(1u, state.upscalingPercentage) : 100u;
}

// temporalReset is left to the caller since it depends on the previous frame
inline FrameConfig Resolve(const RendererState& state, const Support& support, bool rtReflectionsActive)
{
    FrameConfig cfg{};
    cfg.temporal = ResolveTemporal(state, support, rtReflectionsActive);
    cfg.smaa = state.aaType == AntialiasingType::SMAA || state.aaType == AntialiasingType::TAA_SMAA;
    cfg.renderScalePercent = ResolveRenderScalePercent(state, support);

    if (cfg.temporal == Temporal::TAA)
    {
        cfg.jitterPhases = 32;
        cfg.aaOutput = RGResourceID::TAAHistory;
    }
    else if (cfg.IsUpscaler())
    {
        // Vendor guidance: at least 8 * (output / render)^2 phases
        const f32 scaleRatio = 100.0f / static_cast<f32>(cfg.renderScalePercent);
        cfg.jitterPhases = eastl::max(8u, static_cast<u32>(8.0f * scaleRatio * scaleRatio + 0.5f));
        cfg.aaOutput = RGResourceID::TemporalResolve;
    }
    return cfg;
}

// The ReflectionsOnly debug view bypasses AA
inline RGResourceID CompositeInput(const RendererState& state, const FrameConfig& cfg)
{
    const bool reflectionsOnly = mathstl::isFlagSet(state.debugFlags, (u32)DebugFlags::RTEnabled) &&
                                 mathstl::isFlagSet(state.debugFlags, (u32)DebugFlags::RTReflectionsEnabled) &&
                                 state.rt.reflectionsDebugMode == RTReflectionDebugMode::ReflectionsOnly;
    return reflectionsOnly ? RGResourceID::RTReflections : cfg.aaOutput;
}

inline FrameConfig s_current{};

// Config of the frame being recorded; written once per frame by PassManager on the render thread
inline const FrameConfig& Current()
{
    return s_current;
}
inline void SetCurrent(const FrameConfig& config)
{
    s_current = config;
}
} // namespace AA
