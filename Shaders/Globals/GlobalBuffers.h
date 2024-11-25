#extension GL_EXT_nonuniform_qualifier : enable
#include "BindingSlots.h"

layout(set = 1, binding = ViewUBOBindingSlot) uniform ViewUBO
{
    mat4 view;
    mat4 proj;
} ubo;

layout(std140, set = 2, binding = GlobalTransformDataSSBOSlot) readonly buffer GlobalTransformSSBO
{
    mat4 modelMatrices[];
} transformSSBO;