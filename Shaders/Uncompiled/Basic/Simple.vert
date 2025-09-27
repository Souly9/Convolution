#version 450
#define ViewUBOSet 1
#define TransformSSBOSet 2
#define PassPerObjectDataSet 3
#include "../../Globals/GlobalBuffers.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord0;

layout(location = 0) out VertexOut
{
  vec2 fragTexCoord;
} OUT;

void main() {
    //uint transformIdx = perObjectDataSSBO.transformDataIdx[gl_InstanceIndex];
    //uint perObjectDataIdx = perObjectDataSSBO.perObjectDataIdx[gl_InstanceIndex];
    //mat4 worldMat = globalTransformSSBO.modelMatrices[transformIdx];
    gl_Position = vec4(inPosition, 1.0);
    OUT.fragTexCoord = inTexCoord0;
}
