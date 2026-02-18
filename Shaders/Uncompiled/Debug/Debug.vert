#version 450 core
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_shading_language_include : enable
#define ViewUBOSet 1
#define TransformSSBOSet 2
#define PassPerObjectDataSet 3
#include "../../Globals/DrawBuildBuffers.h"
#include "../../Globals/GlobalBuffers.h"
#include "../../Globals/PerObjectBuffers.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord0;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out flat uint outTransformIdx;

void main() {
    uint instanceIdx = perObjectDataSSBO.transformDataIdx[gl_InstanceIndex];
    InstanceData instance = globalInstanceDataSSBO.instances[instanceIdx];
    uint transformIdx = GetTransformIdx(instance);

    gl_Position = ubo.proj * ubo.view * globalTransformSSBO.modelMatrices[transformIdx] * vec4(inPosition, 1.0);
    fragColor = vec3(1,1,0); 
    fragTexCoord = inTexCoord0;
    outTransformIdx = instanceIdx;
}
