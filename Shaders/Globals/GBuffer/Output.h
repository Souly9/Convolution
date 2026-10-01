#ifndef SHADERS_GBUFFER_OUTPUT_H
#define SHADERS_GBUFFER_OUTPUT_H

#define GBUFFER_ALBEDO_OUTPUT_IDX       0
#define GBUFFER_NORMAL_OUTPUT_IDX       1
#define GBUFFER_MATERIAL_OUTPUT_IDX     2
#define GBUFFER_VELOCITY_OUTPUT_IDX     3
#define GBUFFER_ROUGHNESS_OUTPUT_IDX    4
#define GBUFFER_ENTITY_ID_OUTPUT_IDX    5

layout(location = GBUFFER_ALBEDO_OUTPUT_IDX) out vec4 outColor;
layout(location = GBUFFER_NORMAL_OUTPUT_IDX) out vec4 outNormal;
layout(location = GBUFFER_MATERIAL_OUTPUT_IDX) out vec4 outMaterial;
layout(location = GBUFFER_VELOCITY_OUTPUT_IDX) out vec2 outVelocity;
layout(location = GBUFFER_ROUGHNESS_OUTPUT_IDX) out float outRoughness;
layout(location = GBUFFER_ENTITY_ID_OUTPUT_IDX) out uint outEntityID;

FUNC_QUALIFIER void StoreNormalAndMaterialInGBuffer(vec3 normal, uint matIdx)
{
    outNormal = vec4(normal, matIdx);
}
// Resolved here with derivatives so the lighting passes don't resample material textures at mip 0
FUNC_QUALIFIER void StoreMaterialInGBuffer(vec3 emissive, float metallic)
{
    outMaterial = vec4(emissive, metallic);
}

FUNC_QUALIFIER void StoreAlbedoInGBuffer(vec4 albedo)
{
    outColor = albedo;
}

FUNC_QUALIFIER void StoreVelocityInGBuffer(vec2 velocity)
{
    outVelocity = velocity;
}

FUNC_QUALIFIER void StoreEntityIDInGBuffer(uint entityID)
{
    outEntityID = entityID;
}

#endif // SHADERS_GBUFFER_OUTPUT_H