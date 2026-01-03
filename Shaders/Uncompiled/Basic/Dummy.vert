#version 450 core
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_shading_language_include : enable
#define ViewUBOSet 1
#define TransformSSBOSet 2
#define PassPerObjectDataSet 3
#include "../../Globals/GlobalBuffers.h"
#include "../../Globals/DrawBuildBuffers.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord0;

void main() {
   uint instanceIdx = perObjectDataSSBO.transformDataIdx[gl_InstanceIndex];
   InstanceData iData = globalInstanceDataSSBO.instances[instanceIdx];
   uint transformIdx = GetTransformIdx(iData);
   mat4 worldMat = globalTransformSSBO.modelMatrices[transformIdx];
   gl_Position = ubo.proj * ubo.view * worldMat * vec4(inPosition, 1.0);
}