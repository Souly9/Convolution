#version 450 core
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_multiview : require
#extension GL_EXT_scalar_block_layout : enable

#define SharedDataUBOSet     1
#define TransformSSBOSet     2
#define PassPerObjectDataSet 3
#define ShadowViewUBOSet     4

#include "../../Globals/Common.h"
#include "../../Globals/Scene.h"
#include "../../Globals/GeometryPassData.h"
#include "../../Globals/Shadows/Data.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord0;
layout(location = 3) in vec4 inTangent;

void main() {
    mat4 worldMat = FetchWorldMatrix(gl_InstanceIndex);
    mat4 projMat = shadowmapViewUBO.csmViewMatrices[gl_ViewIndex];
    gl_Position = IsInstanceVisible(gl_InstanceIndex) ? (projMat * worldMat * vec4(inPosition, 1.0)) : vec4(0.0 / 0.0);
}
