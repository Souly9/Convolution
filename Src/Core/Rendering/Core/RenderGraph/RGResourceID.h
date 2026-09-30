#pragma once
#include "Core/Global/Typedefs.h"

enum class RGResourceID : u32
{
    Custom = 0,
    MainDepth,
    GBufferAlbedo,
    GBufferNormal,
    GBufferUVMat,
    GBufferDebug,
    GBufferVelocity,
    GBufferThisFrameColor,
    GBufferLastFrameDepth,
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
    TileAssignmentBuffer,
    BloomMip0,
    BloomMip1,
    BloomMip2,
    BloomMip3,
    BloomMip4,
    RTAccumulation,
    CSMShadowMap
};

inline const char* ToString(RGResourceID id)
{
    switch (id)
    {
        case RGResourceID::MainDepth: return "MainDepth";
        case RGResourceID::GBufferAlbedo: return "GBufferAlbedo";
        case RGResourceID::GBufferNormal: return "GBufferNormal";
        case RGResourceID::GBufferUVMat: return "GBufferUVMat";
        case RGResourceID::GBufferDebug: return "GBufferDebug";
        case RGResourceID::GBufferVelocity: return "GBufferVelocity";
        case RGResourceID::GBufferThisFrameColor: return "GBufferThisFrameColor";
        case RGResourceID::GBufferLastFrameDepth: return "GBufferLastFrameDepth";
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
        case RGResourceID::TileAssignmentBuffer: return "TileAssignmentBuffer";
        case RGResourceID::BloomMip0: return "BloomMip0";
        case RGResourceID::BloomMip1: return "BloomMip1";
        case RGResourceID::BloomMip2: return "BloomMip2";
        case RGResourceID::BloomMip3: return "BloomMip3";
        case RGResourceID::BloomMip4: return "BloomMip4";
        case RGResourceID::RTAccumulation: return "RTAccumulation";
        case RGResourceID::CSMShadowMap: return "CSMShadowMap";
        default: return "Custom";
    }
}
