#version 450 core
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_shading_language_include : enable

#define SharedDataUBOSet     1
#define TransformSSBOSet     2
#define PassPerObjectDataSet 3

#include "../../Globals/Common.h"
#include "../../Globals/Bindless.h"
#include "../../Globals/Scene.h"
#include "../../Globals/GeometryPassData.h"

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in flat uint transformIdx;

layout(location = 0) out vec4 outDebug;

void main()
{
    vec4 finalColor = vec4(fragColor, 1.0);

    outDebug = finalColor;
}
