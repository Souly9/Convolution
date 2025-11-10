
layout(set = ShadowViewUBOSet, binding = ShadowMapDataBindingSlot) uniform ShadowmapViewUBO
{
    mat4 csmViewMatrices[MAX_CASCADE_COUNT];
    float cascadeStepSize;
} shadowmapViewUBO;