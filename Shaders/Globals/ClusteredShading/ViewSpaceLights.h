#include "../Types.h"
// ViewSpaceLightsSet must be defined by the including shader
layout(scalar, set = ViewSpaceLightsSet, binding = GlobalViewSpaceLightsSSBOSlot) buffer ViewSpaceLightsSSBO
{
    vec4 lights[MAX_SCENE_LIGHTS]; // xyz=view-space position, w=radius
    uint tileCounters[MAX_TILE_XY];
    uint tileLightIndices[MAX_TILE_XY * MAX_LIGHTS_PER_TILE];
} viewSpaceLights;
