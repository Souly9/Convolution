#ifndef CLUSTERED_SHADING_H
#define CLUSTERED_SHADING_H

#include "Common.h"
#include "Scene.h"

STRUCTDECL(ClusterAABB)
    STRUCTFIELD(vec4, minBounds)
    STRUCTFIELD(vec4, maxBounds)
STRUCTEND()

STRUCTDECL(ClusterGridBuffer)
    STRUCTFIELD_ARRAY(ClusterAABB, clusters, MAX_CLUSTERS)
STRUCTEND()

STRUCTDECL(ViewSpaceLightsBuffer)
    STRUCTFIELD_ARRAY(vec4, lights, MAX_SCENE_LIGHTS) // xyz=view-space position, w=radius
    STRUCTFIELD_ARRAY(uint, tileCounters, MAX_TILE_XY)
    STRUCTFIELD_ARRAY(uint, tileLightIndices, MAX_TILE_XY * MAX_LIGHTS_PER_TILE)
STRUCTEND()

#ifndef __cplusplus
layout(scalar, set = ClusterGridSet, binding = ClusterGridSSBOSlot) buffer ClusterGrid
{
    ClusterGridBuffer clusterGrid;
};

layout(scalar, set = ViewSpaceLightsSet, binding = GlobalViewSpaceLightsSSBOSlot) buffer ViewSpaceLightsSSBO
{
    ViewSpaceLightsBuffer viewSpaceLights;
};
#endif

#endif // SHADERS_CLUSTERED_SHADING_H
