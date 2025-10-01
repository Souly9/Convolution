#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shading_language_include : enable

layout(location = 0) in vec3 inPosition;
layout(location = 2) in vec2 inTexCoord0;

layout(location = 0) out VertexOut
{
  vec2 fragTexCoord;
} OUT;

void main() {
    //uint transformIdx = perObjectDataSSBO.transformDataIdx[gl_InstanceIndex];
    //uint perObjectDataIdx = perObjectDataSSBO.perObjectDataIdx[gl_InstanceIndex];
    //mat4 worldMat = globalTransformSSBO.modelMatrices[transformIdx];
    OUT.fragTexCoord = inTexCoord0;
    gl_Position = vec4(inPosition, 1.0);
}
