#ifndef SHADERS_SHADOWS_H
#define SHADERS_SHADOWS_H

layout(scalar, set = ShadowViewUBOSet, binding = ShadowMapDataBindingSlot) uniform ShadowmapViewUBO
{
    mat4 csmViewMatrices[16];
    vec4 cascadeSplits[4];
    int cascadeCount;
    int screenSpaceShadows;
}
shadowmapViewUBO;

#endif // SHADERS_SHADOWS_H
 

