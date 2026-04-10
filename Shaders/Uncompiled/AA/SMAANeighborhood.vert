#version 450 core
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec3 inTangent;

layout(location = 0) out vec2 texCoord;
layout(location = 1) out vec4 offset;

#define SMAA_INCLUDE_VS 1
#include "SMAA_Common.h"
#include "SMAA.hlsl"

void main()
{
    texCoord = inTexCoord;
    SMAANeighborhoodBlendingVS(texCoord, offset);
    gl_Position = vec4(inPosition, 1.0);
}
