#ifndef SHADERS_GBUFFER_SAMPLING_H
#define SHADERS_GBUFFER_SAMPLING_H

#define GBUFFER_ALBEDO_OUTPUT_IDX       0
#define GBUFFER_NORMAL_OUTPUT_IDX       1
#define GBUFFER_MAT_TEXCOORD_OUTPUT_IDX 2
#define GBUFFER_VELOCITY_OUTPUT_IDX     3
#define GBUFFER_ROUGHNESS_OUTPUT_IDX    4

layout(location = GBUFFER_ALBEDO_OUTPUT_IDX) out vec4 outColor;
layout(location = GBUFFER_NORMAL_OUTPUT_IDX) out vec4 outNormal;
layout(location = GBUFFER_MAT_TEXCOORD_OUTPUT_IDX) out vec4 outTexCoordMat;
layout(location = GBUFFER_VELOCITY_OUTPUT_IDX) out vec2 outVelocity;
layout(location = GBUFFER_ROUGHNESS_OUTPUT_IDX) out float outRoughness;

FUNC_QUALIFIER void StoreNormalAndMaterialInGBuffer(vec3 normal, uint matIdx)
{
    outNormal = vec4(normal, matIdx);
}
FUNC_QUALIFIER void StoreTexCoordInGBuffer(vec2 uvs)
{
    outTexCoordMat = vec4(uvs, 0, 0);
}

FUNC_QUALIFIER void StoreAlbedoInGBuffer(vec4 albedo)
{
    outColor = albedo;
}

FUNC_QUALIFIER void StoreVelocityInGBuffer(vec2 velocity)
{
    outVelocity = velocity;
}

#endif // SHADERS_GBUFFER_SAMPLING_H