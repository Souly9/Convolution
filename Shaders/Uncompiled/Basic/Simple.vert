#version 450
#define ViewUBOSet 1
#define TransformSSBOSet 2
#define PassPerObjectDataSet 4
#include "../../Globals/GlobalBuffers.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord0;

layout(location = 0) out VertexOut
{
  vec4 worldPos;
  vec3 normal;
  vec2 fragTexCoord;
  Material mat;
} OUT;

void main() {
    uint transformIdx = perObjectDataSSBO.transformDataIdx[gl_InstanceIndex];
    uint perObjectDataIdx = perObjectDataSSBO.perObjectDataIdx[gl_InstanceIndex];
    mat4 worldMat = globalTransformSSBO.modelMatrices[transformIdx];
    OUT.worldPos = worldMat * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * OUT.worldPos;
    OUT.fragTexCoord = inTexCoord0;
    // Supposedly better to take adjugate from world matrix
    OUT.normal = AdjugateFromWorldMat(worldMat) * normalize(inNormal);
    OUT.mat = globalObjectDataSSBO.data[perObjectDataIdx];
}
