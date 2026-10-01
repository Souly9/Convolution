#ifndef SHADERS_BINDLESS_H
#define SHADERS_BINDLESS_H

#extension GL_EXT_nonuniform_qualifier : require

#include "Common.h"

// Texture-only arrays; combined image samplers would each count against the per-stage sampler limit
layout(set = 0, binding = GlobalBindlessTextureBufferSlot) uniform texture2D GlobalBindlessTextures[];
layout(set = 0, binding = GlobalBindlessArrayTextureBufferSlot) uniform texture2DArray GlobalBindlessArrayTextures[];
#ifndef BindlessImageSet
#define BindlessImageSet 0
#endif
layout(set = BindlessImageSet, binding = GlobalBindlessImageBufferSlot, rgba16f) uniform image2D GlobalBindlessImages[];
layout(set = 0, binding = GlobalSamplerSlot) uniform sampler GlobalSamplers[GLOBAL_SAMPLER_COUNT];

// Bindless texture with a SAMPLER_* global sampler; the spec wants NonUniform on the combined sampler too
#define BindlessTexture2D(idx, samplerIdx) \
    nonuniformEXT(sampler2D(GlobalBindlessTextures[nonuniformEXT(idx)], GlobalSamplers[samplerIdx]))
#define BindlessTextureArray(idx, samplerIdx) \
    nonuniformEXT(sampler2DArray(GlobalBindlessArrayTextures[nonuniformEXT(idx)], GlobalSamplers[samplerIdx]))

vec4 SampleLinearClamp(uint texIdx, vec2 uv)
{
    return textureLod(BindlessTexture2D(texIdx, SAMPLER_LINEAR_CLAMP), uv, 0.0);
}

vec4 SamplePointClamp(uint texIdx, vec2 uv)
{
    return textureLod(BindlessTexture2D(texIdx, SAMPLER_POINT_CLAMP), uv, 0.0);
}

#endif // SHADERS_BINDLESS_H
