#version 450 core
#extension GL_ARB_shading_language_include : enable
layout(location = 0) in vec3 fragColor;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor,1);
}