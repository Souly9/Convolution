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
// Emissive in rgb, metallic in a
STRUCTFIELD(BindlessTextureHandle, gbufferMaterialIdx)
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
STRUCTFIELD(BindlessTextureHandle, gbufferRoughnessIdx)
// 0 when DebugShapePass didn't run this frame
STRUCTFIELD(BindlessTextureHandle, debugOverlayIdx)
// std140 rounds the block to 16 bytes, keep the C++ size equal
STRUCTFIELD(BindlessTextureHandle, pad1)
STRUCTFIELD(BindlessTextureHandle, pad2)
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
