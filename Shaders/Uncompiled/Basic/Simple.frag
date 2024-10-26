#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#include "../../Globals/BindingSlots.h"
layout(set = 0, binding = GlobalBindlessTextureBufferSlot) uniform sampler2D GlobalBindlessTextures[];

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(GlobalBindlessTextures[0], fragTexCoord);
}
