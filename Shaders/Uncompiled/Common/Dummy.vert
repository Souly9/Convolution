#version 450
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#define SharedDataUBOSet     1
#define TransformSSBOSet     2
#define PassPerObjectDataSet 3

#include "../../Globals/Common.h"
#include "../../Globals/Scene.h"
#include "../../Globals/GeometryPassData.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord0;
layout(location = 3) in vec4 inTangent;

void main()
{
    mat4 worldMat = FetchWorldMatrix(gl_InstanceIndex);
    gl_Position = ubo.jitteredProjection * worldMat * vec4(inPosition, 1.0);
}
