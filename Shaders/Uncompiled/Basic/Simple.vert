#version 450
#include "../../Globals/GlobalBuffers.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord0;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    gl_Position = ubo.proj * ubo.view * transformSSBO.modelMatrices[0] * vec4(inPosition, 1.0);
    fragColor = inColor;
    fragTexCoord = inTexCoord0;
}
