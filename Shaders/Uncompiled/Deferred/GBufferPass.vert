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
#include "../../Globals/GeometryHelpers.h"
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
    vec4 prevUnjitteredClipPos;
    vec3 worldNormal;
    vec2 fragTexCoord;
    flat uint matIdx;
}
OUT;

void main()
{
    uint instanceIdx = perObjectDataSSBO.transformDataIdx[gl_InstanceIndex];
    InstanceData iData = FetchInstanceData(instanceIdx);
    uint transformIdx = GetTransformIdx(iData);
    mat4 worldMat = FetchInstanceWorldMatrix(iData);
    vec3 localNormal = inNormal;
    OUT.worldNormal = TransformLocalNormalToWorld(worldMat, localNormal);

    OUT.TBN = BuildWorldTBN(worldMat, OUT.worldNormal, inTangent);
    OUT.matIdx = GetMaterialIdx(iData);
    OUT.fragTexCoord = inTexCoord0;

    vec4 localPosition = vec4(inPosition, 1.0);
    OUT.unjitteredClipPos = ubo.viewProjection * worldMat * localPosition;
    OUT.jitteredClipPos = ApplyFrameJitter(OUT.unjitteredClipPos);
    OUT.prevUnjitteredClipPos =
        ubo.prevViewProjection * prevGlobalTransformSSBO.prevModelMatrices[transformIdx] * localPosition;
    gl_Position = IsVisible(iData) ? OUT.jitteredClipPos : vec4(0.0 / 0.0);
}
