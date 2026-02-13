#version 450
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#define ViewUBOSet     0
#define ClusterGridSet 1

#include "../../Globals/ClusteredShading/AABBs.h"
#include "../../Globals/GlobalBuffers.h"

layout(location = 0) out vec3 outColor;
// Corners of a unit cube (0..1)
const vec3 corners[8] = vec3[](vec3(0, 0, 0),
                               vec3(1, 0, 0),
                               vec3(1, 1, 0),
                               vec3(0, 1, 0),
                               vec3(0, 0, 1),
                               vec3(1, 0, 1),
                               vec3(1, 1, 1),
                               vec3(0, 1, 1));

void main()
{
    // We use the index buffer from ClusterDebugPass which contains the corner indices directly.
    // 0..7 map to the 8 corners.
    uint cornerIndex = gl_VertexIndex;

    uint clusterIdx = gl_InstanceIndex;

    ClusterAABB cluster = clusterGrid.clusters[clusterIdx];

    vec3 minB = cluster.minBounds.xyz;
    vec3 maxB = cluster.maxBounds.xyz;
    vec3 size = maxB - minB;

    vec3 pos = minB + corners[cornerIndex] * size;

    gl_Position = ubo.proj * vec4(pos, 1.0);
    outColor = vec3(0.0, 1.0, 0.0); // Green
}
