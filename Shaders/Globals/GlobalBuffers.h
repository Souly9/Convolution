#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_scalar_block_layout : require
#include "BindingSlots.h"
#include "Types.h"

#include "DebugFlags.h"

layout(scalar, set = SharedDataUBOSet, binding = SharedDataUBOBindingSlot) uniform SharedDataUBO
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 viewInverse;
    mat4 projectionInverse;
    vec4 viewPos;

    // Debug & Global settings
    uint debugFlags;
    int debugViewMode;
    float exposure;
    int toneMapperType;
    float ambientIntensity;
    float gt7PaperWhite;
    float gt7ReferenceLuminance;
}
ubo;

mat3 AdjugateFromWorldMat(mat4 m)
{
    return mat3(cross(m[1].xyz, m[2].xyz),
        cross(m[2].xyz, m[0].xyz),
        cross(m[0].xyz, m[1].xyz));
}
