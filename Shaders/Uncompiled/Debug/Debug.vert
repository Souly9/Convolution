#version 450 core
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_shading_language_include : enable
#define ViewUBOSet 0
#define TransformSSBOSet 1
#define PassPerObjectDataSet 2
#include "../../Globals/GlobalBuffers.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord0;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    uint transformIdx = perObjectDataSSBO.transformDataIdx[gl_InstanceIndex];
    uint perObjectDataIdx = perObjectDataSSBO.perObjectDataIdx[gl_InstanceIndex];
    gl_Position = ubo.proj * ubo.view * globalTransformSSBO.modelMatrices[transformIdx] * vec4(inPosition, 1.0);
    fragColor = globalObjectDataSSBO.data[perObjectDataIdx].baseColor.xyz;
}
