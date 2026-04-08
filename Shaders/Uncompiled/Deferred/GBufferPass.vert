#version 450 core
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#define SharedDataUBOSet     1
#define TransformSSBOSet     2
#define PassPerObjectDataSet 3

#include "../../Globals/Bindless.h"
#include "../../Globals/Common.h"
#include "../../Globals/DebugPrintf.h"
#include "../../Globals/GeometryPassData.h"


layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord0;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out VertexOut
{
    mat3 TBN;
    vec4 jitteredClipPos;
    vec4 unjitteredClipPos;
    vec4 prevClipPos;
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
    mat3 normalMat = AdjugateFromWorldMat(worldMat);
    vec3 localNormal = inNormal;
    OUT.worldNormal = normalize(normalMat * localNormal);
    vec3 worldTangent = normalize(normalMat * inTangent.xyz);
    vec3 worldBitangent = cross(OUT.worldNormal, worldTangent) * inTangent.w;

    OUT.TBN = mat3(worldTangent, worldBitangent, OUT.worldNormal);
    OUT.matIdx = GetMaterialIdx(iData);
    OUT.fragTexCoord = inTexCoord0;

    OUT.unjitteredClipPos = ubo.viewProjection * worldMat * vec4(inPosition, 1.0);
    OUT.prevClipPos =
        ubo.prevViewProjection * prevGlobalTransformSSBO.prevModelMatrices[transformIdx] * vec4(inPosition, 1.0);
    gl_Position = IsVisible(iData) ? (ubo.jitteredProjection * worldMat * vec4(inPosition, 1.0)) : vec4(0.0 / 0.0);
}
