#include "GBuffer.h"

layout(location = GBUFFER_ALBEDO_OUTPUT_IDX) out vec4 outColor;
layout(location = GBUFFER_NORMAL_OUTPUT_IDX) out vec4 outNormal;
layout(location = GBUFFER_MAT_TEXCOORD_OUTPUT_IDX) out vec4 outTexCoordMat;

void StoreNormalAndMaterialInGBuffer(vec3 normal, uint matIdx)
{
    outNormal = vec4(normal, matIdx);
}

void StoreAlbedoInGBuffer(vec4 albedo)
{
    outColor = albedo;
}
