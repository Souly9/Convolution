#ifndef SMAA_COMMON_H
#define SMAA_COMMON_H

#define SMAA_CUSTOM_SL 1

#ifdef SMAA_INCLUDE_VS
    #ifndef SMAA_INCLUDE_PS
    #define SMAA_INCLUDE_PS 0
    #endif
#endif

#ifdef SMAA_INCLUDE_PS
    #ifndef SMAA_INCLUDE_VS
    #define SMAA_INCLUDE_VS 0
    #endif
#endif

#include "Bindless.h"

// GLSL to HLSL types for SMAA
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define int2 ivec2
#define int3 ivec3
#define int4 ivec4
#define bool2 bvec2
#define bool3 bvec3
#define bool4 bvec4

#define lerp(a, b, t) mix(a, b, t)
#define saturate(a) clamp(a, 0.0, 1.0)
#define mad(a, b, c) (a * b + c)
#define SMAA_FLATTEN
#define SMAA_BRANCH

// SMAA Porting for Bindless
#define SMAATexture2D(tex) uint tex
#define SMAATexture2DMS(tex) uint tex
#define SMAATexture2DMS2(tex) uint tex
#define SMAATexturePass2D(tex) tex
#define SMAASampleLevelZero(tex, coord) textureLod(GlobalBindlessTextures[nonuniformEXT(tex)], coord, 0.0)
#define SMAASampleLevelZeroOffset(tex, coord, offset) textureLodOffset(GlobalBindlessTextures[nonuniformEXT(tex)], coord, 0.0, offset)
#define SMAASample(tex, coord) texture(GlobalBindlessTextures[nonuniformEXT(tex)], coord)
#define SMAASamplePoint(tex, coord) texture(GlobalBindlessTextures[nonuniformEXT(tex)], coord)
#define SMAAGather(tex, coord) textureGather(GlobalBindlessTextures[nonuniformEXT(tex)], coord)
#define SMAALoad(tex, pos, sample) texelFetch(GlobalBindlessTextures[nonuniformEXT(tex)], pos, sample)

#define SMAA_AREATEX_SELECT(sample) sample.rg
#define SMAA_SEARCHTEX_SELECT(sample) sample.r

#ifndef SMAA_RT_METRICS
layout(push_constant, scalar) uniform PushConsts {
    vec4 metrics;
    uint tex1;
    uint tex2;
    uint tex3;
} pc;

#define SMAA_RT_METRICS pc.metrics
#endif

#endif // SMAA_COMMON_H
