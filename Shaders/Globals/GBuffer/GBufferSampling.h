#ifndef SHADERS_GBUFFER_SAMPLING_H
#define SHADERS_GBUFFER_SAMPLING_H

#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(set = GBufferUBOSet, binding = GlobalGBufferPostProcessUBOSlot) uniform GBufferUBO
{
    BindlessTextureHandle gbufferAlbedoIdx;
    BindlessTextureHandle gbufferNormalIdx;
    BindlessTextureHandle gbufferTexCoordMatIdx;
    BindlessTextureHandle gbufferDebugIdx;
    BindlessTextureHandle gbufferVelocityIdx;
    BindlessTextureHandle depthBufferIdx;
    BindlessTextureHandle lastFrameColorBufferIdx;
    BindlessTextureHandle thisFrameColorBufferIdx;
    BindlessTextureHandle lastFrameDepthIdx;
    BindlessTextureHandle gbufferResolveIdx;
}
gbufferUBO;

layout(set = GBufferUBOSet, binding = GlobalShadowMapUBOSlot) uniform ShadowMapUBO
{
    BindlessTextureHandle directionalShadowMapIdx;
}
shadowmapUBO;

#endif // SHADERS_GBUFFER_SAMPLING_H
 
