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
    STRUCTFIELD(BindlessTextureHandle, gbufferDebugIdx)
    STRUCTFIELD(BindlessTextureHandle, gbufferVelocityIdx)
    STRUCTFIELD(BindlessTextureHandle, depthBufferIdx)
    STRUCTFIELD(BindlessTextureHandle, lastFrameColorBufferIdx)
    STRUCTFIELD(BindlessTextureHandle, thisFrameColorBufferIdx)
    STRUCTFIELD(BindlessTextureHandle, lastFrameDepthIdx)
    STRUCTFIELD(BindlessTextureHandle, gbufferResolveIdx)
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
 
