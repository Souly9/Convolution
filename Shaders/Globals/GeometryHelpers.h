#ifndef SHADERS_GEOMETRY_HELPERS_H
#define SHADERS_GEOMETRY_HELPERS_H

#include "GeometryPassData.h"
#include "Utils/ScreenSpace.h"

STRUCTDECL(GPUCompleteVertex)
    STRUCTFIELD(vec3, position)
    STRUCTFIELD(vec3, normal)
    STRUCTFIELD(vec2, texCoord)
    STRUCTFIELD(vec4, tangent)
STRUCTEND()

#ifndef __cplusplus
FUNC_QUALIFIER InstanceData FetchInstanceData(uint instanceDataIdx)
{
    return globalInstanceDataSSBO.instances[instanceDataIdx];
}

FUNC_QUALIFIER Material FetchInstanceMaterial(InstanceData instance)
{
    return globalObjectDataSSBO.materials[GetMaterialIdx(instance)];
}

FUNC_QUALIFIER mat4 FetchInstanceWorldMatrix(InstanceData instance)
{
    return globalTransformSSBO.modelMatrices[GetTransformIdx(instance)];
}

FUNC_QUALIFIER vec3 TransformLocalNormalToWorld(mat4 worldMat, vec3 localNormal)
{
    return normalize(AdjugateFromWorldMat(worldMat) * localNormal);
}

FUNC_QUALIFIER mat3 BuildWorldTBN(mat4 worldMat, vec3 worldNormal, vec4 localTangent)
{
    vec3 worldTangent = normalize(mat3(worldMat) * localTangent.xyz);
    vec3 worldBitangent = normalize(cross(worldNormal, worldTangent) * localTangent.w);
    return mat3(worldTangent, worldBitangent, worldNormal);
}
#endif

#endif // SHADERS_GEOMETRY_HELPERS_H
