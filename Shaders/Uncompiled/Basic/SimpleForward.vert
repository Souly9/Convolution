#version 450 core
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
#include "../../Globals/GBufferPass.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord0;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out VertexOut
{
    mat3 TBN;
    vec4 worldPos;
    vec3 worldNormal;
    vec2 fragTexCoord;
    flat uint matIdx;
}
OUT;

void main()
{
    uint instanceIdx = perObjectDataSSBO.transformDataIdx[gl_InstanceIndex];
    InstanceData iData = globalInstanceDataSSBO.instances[instanceIdx];
    uint transformIdx = GetTransformIdx(iData);
    mat4 worldMat = globalTransformSSBO.modelMatrices[transformIdx];
    OUT.worldPos = worldMat * vec4(inPosition, 1.0);
    OUT.fragTexCoord = inTexCoord0;
    mat3 normalMat = AdjugateFromWorldMat(worldMat);
    OUT.worldNormal = normalize(normalMat * inNormal);
    //OUT.worldNormal.y *= -inTangent.w;
    vec3 worldTangent = normalize(normalMat * inTangent.xyz);
    vec3 worldBitangent = cross(OUT.worldNormal, worldTangent) * inTangent.w;
    OUT.TBN = mat3(worldTangent, worldBitangent, OUT.worldNormal);
    OUT.matIdx = GetMaterialIdx(iData);
    gl_Position = ubo.viewProjection * OUT.worldPos;
}
