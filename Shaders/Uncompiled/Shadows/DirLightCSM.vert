#version 450 core
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_multiview : require

#define ViewUBOSet 1
#define TransformSSBOSet 2
#define PassPerObjectDataSet 3
#define ShadowViewUBOSet 4
#include "../../Globals/DrawBuildBuffers.h"
#include "../../Globals/GlobalBuffers.h"
#include "../../Globals/PerObjectBuffers.h"
#include "../../Globals/ShadowBuffers.h"
#include "../../Globals/Utils/InstanceUtils.h"


layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord0;

void main() {
    mat4 worldMat = FetchWorldMatrix(gl_InstanceIndex);
    mat4 projMat = shadowmapViewUBO.csmViewMatrices[gl_ViewIndex];
    gl_Position = projMat * worldMat * vec4(inPosition, 1.0);
}