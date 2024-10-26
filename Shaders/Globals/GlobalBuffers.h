#extension GL_EXT_nonuniform_qualifier : enable
#include "BindingSlots.h"

layout(set = 1, binding = ViewUBOBindingSlot) uniform ViewUBO
{
    mat4 view;
    mat4 proj;
    mat4 model;
} ubo;