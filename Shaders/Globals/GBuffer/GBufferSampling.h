#ifndef SHADERS_GBUFFER_SAMPLING_H
#define SHADERS_GBUFFER_SAMPLING_H

#include "../Common.h"

#ifndef __cplusplus
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#endif

STRUCTDECL(GBufferPostProcessUBO)
STRUCTFIELD(BindlessTextureHandle, gbufferAlbedoIdx)
STRUCTFIELD(BindlessTextureHandle, gbufferNormalIdx)
STRUCTFIELD(BindlessTextureHandle, gbufferTexCoordMatIdx)
STRUCTFIELD(BindlessTextureHandle, gbufferVelocityIdx)
STRUCTFIELD(BindlessTextureHandle, depthBufferIdx)
STRUCTFIELD(BindlessTextureHandle, lastFrameDepthIdx)
// Lit HDR scene color at render resolution
STRUCTFIELD(BindlessTextureHandle, sceneColorIdx)
// TAA ping-pong: previous frame (sampled) and this frame (storage)
STRUCTFIELD(BindlessTextureHandle, taaHistoryIdx)
STRUCTFIELD(BindlessTextureHandle, taaOutputIdx)
// HDR color the composite tonemaps
STRUCTFIELD(BindlessTextureHandle, compositeInputIdx)
STRUCTFIELD(BindlessTextureHandle, rtDebugViewIdx)
STRUCTFIELD(BindlessTextureHandle, bloomResultIdx)
STRUCTEND()

STRUCTDECL(ShadowMapUBO)
STRUCTFIELD(BindlessTextureHandle, directionalShadowMapIdx)
STRUCTEND()

#ifndef __cplusplus
layout(set = GBufferUBOSet, binding = GlobalGBufferPostProcessUBOSlot) uniform GBufferUBOBlock
{
    GBufferPostProcessUBO gbufferUBO;
};

layout(set = GBufferUBOSet, binding = GlobalShadowMapUBOSlot) uniform ShadowMapUBOBlock
{
    ShadowMapUBO shadowmapUBO;
};
#endif

#endif // SHADERS_GBUFFER_SAMPLING_H
