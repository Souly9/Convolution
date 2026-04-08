#ifndef CLUSTERED_SHADING_H
#define CLUSTERED_SHADING_H

#include "Common.h"
#include "Scene.h"

struct ClusterAABB
{
    vec4 minBounds;
    vec4 maxBounds;
};

layout(scalar, set = ClusterGridSet, binding = ClusterGridSSBOSlot) buffer ClusterGrid
{
    ClusterAABB clusters[];
} clusterGrid;

layout(scalar, set = ViewSpaceLightsSet, binding = GlobalViewSpaceLightsSSBOSlot) buffer ViewSpaceLightsSSBO
{
    vec4 lights[MAX_SCENE_LIGHTS]; // xyz=view-space position, w=radius
    uint tileCounters[MAX_TILE_XY];
    uint tileLightIndices[MAX_TILE_XY * MAX_LIGHTS_PER_TILE];
} viewSpaceLights;

#endif // SHADERS_CLUSTERED_SHADING_H
