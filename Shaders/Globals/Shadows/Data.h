#ifndef SHADERS_SHADOWS_H
#define SHADERS_SHADOWS_H

#include "../Common.h"

STRUCTDECL(ShadowmapViewUBO)
    STRUCTFIELD_ARRAY(mat4, csmViewMatrices, 16)
    STRUCTFIELD_ARRAY(vec4, cascadeSplits, 4)
    STRUCTFIELD(int, cascadeCount)
    STRUCTFIELD(BindlessTextureHandle, screenSpaceShadows)
STRUCTEND()

#ifndef __cplusplus
layout(scalar, set = ShadowViewUBOSet, binding = ShadowMapDataBindingSlot) uniform ShadowmapViewUBOBlock
{
    ShadowmapViewUBO shadowmapViewUBO;
};
#endif

#endif // SHADERS_SHADOWS_H
 

