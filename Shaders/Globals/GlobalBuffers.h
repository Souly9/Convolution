#extension GL_EXT_nonuniform_qualifier : require
#include "BindingSlots.h"

layout(set = ViewUBOSet, binding = ViewUBOBindingSlot) uniform ViewUBO
{
    mat4 view;
    mat4 proj;
    mat4 viewProjection;
    mat4 viewInverse;
    mat4 projectionInverse;
    vec4 viewPos;
}
ubo;

mat3 AdjugateFromWorldMat(mat4 m)
{
    return mat3(cross(m[1].xyz, m[2].xyz),
        cross(m[2].xyz, m[0].xyz),
        cross(m[0].xyz, m[1].xyz));
}