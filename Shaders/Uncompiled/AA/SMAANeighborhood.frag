#version 450 core
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec2 texCoord;
layout(location = 1) in vec4 offset;

layout(location = 0) out vec4 outColor;

#define SMAA_INCLUDE_PS 1
#include "SMAA_Common.h"
#include "SMAA.hlsl"

void main()
{
    outColor = SMAANeighborhoodBlendingPS(texCoord, offset, pc.tex1, pc.tex2);
}
