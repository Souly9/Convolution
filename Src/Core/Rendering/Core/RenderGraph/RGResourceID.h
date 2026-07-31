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
    GBufferPostAAColor,
    GBufferRoughness,
    RTReflections,
    RTAOOutput,
    ScreenSpaceShadows,
    SMAAEdges,
    SMAABlend,
    DLSSExposure,
    Swapchain,
    TileAssignmentBuffer,
    BloomDownsample,
    BloomResult
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
        case RGResourceID::GBufferPostAAColor: return "GBufferPostAAColor";
        case RGResourceID::GBufferRoughness: return "GBufferRoughness";
        case RGResourceID::RTReflections: return "RTReflections";
        case RGResourceID::RTAOOutput: return "RTAOOutput";
        case RGResourceID::ScreenSpaceShadows: return "ScreenSpaceShadows";
        case RGResourceID::SMAAEdges: return "SMAAEdges";
        case RGResourceID::SMAABlend: return "SMAABlend";
        case RGResourceID::DLSSExposure: return "DLSSExposure";
        case RGResourceID::Swapchain: return "Swapchain";
        case RGResourceID::TileAssignmentBuffer: return "TileAssignmentBuffer";
        case RGResourceID::BloomDownsample: return "BloomDownsample";
        case RGResourceID::BloomResult: return "BloomResult";
        default: return "CustomResource";
    }
}
