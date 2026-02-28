#include "Types.h"

#define GBUFFER_ALBEDO_OUTPUT_IDX 0
#define GBUFFER_NORMAL_OUTPUT_IDX 1
#define GBUFFER_MAT_TEXCOORD_OUTPUT_IDX 2

layout(location = GBUFFER_ALBEDO_OUTPUT_IDX) out vec4 outColor;
layout(location = GBUFFER_NORMAL_OUTPUT_IDX) out vec4 outNormal;
layout(location = GBUFFER_MAT_TEXCOORD_OUTPUT_IDX) out vec4 outTexCoordMat;

void StoreNormalAndMaterialInGBuffer(vec3 normal, uint matIdx)
{
    outNormal = vec4(normal, matIdx);
}
void StoreTexCoordInGBuffer(vec2 uvs)
{
    outTexCoordMat = vec4(uvs, 0, 0);
}

void StoreAlbedoInGBuffer(vec4 albedo)
{
    outColor = albedo;
}
