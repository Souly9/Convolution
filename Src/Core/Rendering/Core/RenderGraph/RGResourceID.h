#pragma once
#include "Core/Global/Typedefs.h"

enum class RGResourceID : u32
{
    Custom = 0,
    MainDepth,
    GBufferAlbedo,
    GBufferNormal,
    GBufferMaterial,
    GBufferDebug,
    GBufferVelocity,
    GBufferThisFrameColor,
    TemporalResolve,
    TAAHistory,
    GBufferPostAAColor,
    GBufferRoughness,
    GBufferEntityID,
    RTReflections,
    RTAOOutput,
    ScreenSpaceShadows,
    SMAAEdges,
    SMAABlend,
    DLSSExposure,
    Swapchain,
    // Clustered lighting buffers: light lists (+ uploaded lights), view-space lights and tiles, cluster AABBs
    LightClusterBuffer,
    ViewSpaceLightsBuffer,
    ClusterGridBuffer,
    BloomMip0,
    BloomMip1,
    BloomMip2,
    BloomMip3,
    BloomMip4,
    RTAccumulation,
    CSMShadowMap,
    // Unlit debug shapes the composite draws over the tonemapped scene
    DebugOverlay
};

inline const char* ToString(RGResourceID id)
{
    switch (id)
    {
        case RGResourceID::MainDepth: return "MainDepth";
        case RGResourceID::GBufferAlbedo: return "GBufferAlbedo";
        case RGResourceID::GBufferNormal: return "GBufferNormal";
        case RGResourceID::GBufferMaterial: return "GBufferMaterial";
        case RGResourceID::GBufferDebug: return "GBufferDebug";
        case RGResourceID::GBufferVelocity: return "GBufferVelocity";
        case RGResourceID::GBufferThisFrameColor: return "GBufferThisFrameColor";
        case RGResourceID::TemporalResolve: return "TemporalResolve";
        case RGResourceID::TAAHistory: return "TAAHistory";
        case RGResourceID::GBufferPostAAColor: return "GBufferPostAAColor";
        case RGResourceID::GBufferRoughness: return "GBufferRoughness";
        case RGResourceID::GBufferEntityID: return "GBufferEntityID";
        case RGResourceID::RTReflections: return "RTReflections";
        case RGResourceID::RTAOOutput: return "RTAOOutput";
        case RGResourceID::ScreenSpaceShadows: return "ScreenSpaceShadows";
        case RGResourceID::SMAAEdges: return "SMAAEdges";
        case RGResourceID::SMAABlend: return "SMAABlend";
        case RGResourceID::DLSSExposure: return "DLSSExposure";
        case RGResourceID::Swapchain: return "Swapchain";
        case RGResourceID::LightClusterBuffer: return "LightClusterBuffer";
        case RGResourceID::ViewSpaceLightsBuffer: return "ViewSpaceLightsBuffer";
        case RGResourceID::ClusterGridBuffer: return "ClusterGridBuffer";
        case RGResourceID::BloomMip0: return "BloomMip0";
        case RGResourceID::BloomMip1: return "BloomMip1";
        case RGResourceID::BloomMip2: return "BloomMip2";
        case RGResourceID::BloomMip3: return "BloomMip3";
        case RGResourceID::BloomMip4: return "BloomMip4";
        case RGResourceID::RTAccumulation: return "RTAccumulation";
        case RGResourceID::DebugOverlay: return "DebugOverlay";
        case RGResourceID::CSMShadowMap: return "CSMShadowMap";
        default: return "Custom";
    }
}
