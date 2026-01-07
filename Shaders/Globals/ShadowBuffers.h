
layout(set = ShadowViewUBOSet, binding = ShadowMapDataBindingSlot) uniform ShadowmapViewUBO
{
    mat4 csmViewMatrices[16];
    vec4 cascadeSplits[4];
    int cascadeCount;
}
shadowmapViewUBO;