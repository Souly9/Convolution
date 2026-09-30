#ifndef SHADERS_BINDLESS_H
#define SHADERS_BINDLESS_H

#extension GL_EXT_nonuniform_qualifier : require

#include "Common.h"

layout(set = 0, binding = GlobalBindlessTextureBufferSlot) uniform sampler2D GlobalBindlessTextures[];
layout(set = 0, binding = GlobalBindlessArrayTextureBufferSlot) uniform sampler2DArray GlobalBindlessArrayTextures[];
#ifndef BindlessImageSet
#define BindlessImageSet 0
#endif
layout(set = BindlessImageSet, binding = GlobalBindlessImageBufferSlot, rgba16f) uniform image2D GlobalBindlessImages[];
// Same indices as GlobalBindlessTextures (2D only), combined with GlobalSamplers by the shader
layout(set = 0, binding = GlobalBindlessSampledTextureSlot) uniform texture2D GlobalBindlessSampledTextures[];
layout(set = 0, binding = GlobalSamplerSlot) uniform sampler GlobalSamplers[GLOBAL_SAMPLER_COUNT];

vec4 SampleLinearClamp(uint texIdx, vec2 uv)
{
    return textureLod(
        sampler2D(GlobalBindlessSampledTextures[nonuniformEXT(texIdx)], GlobalSamplers[SAMPLER_LINEAR_CLAMP]), uv, 0.0);
}

vec4 SamplePointClamp(uint texIdx, vec2 uv)
{
    return textureLod(
        sampler2D(GlobalBindlessSampledTextures[nonuniformEXT(texIdx)], GlobalSamplers[SAMPLER_POINT_CLAMP]), uv, 0.0);
}

#endif // SHADERS_BINDLESS_H
 

