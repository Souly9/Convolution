#ifndef SHADERS_CLUSTER_LIGHT_DATA_H
#define SHADERS_CLUSTER_LIGHT_DATA_H

layout(scalar, set = TileArraySet, binding = GlobalTileArraySSBOSlot) buffer GlobalTileSSBO
{
    DirectionalLight dirLight;
    uint numLights;
    Light lights[MAX_SCENE_LIGHTS];

    // Cluster light indices
    uint clusterOffsets[MAX_CLUSTERS + 1];
    uint clusterLightIndices[MAX_LIGHT_INDICES];
}
lightData;
// IO Helpers
FUNC_QUALIFIER uint GetClusterLightBaseIndex(uint clusterIdx)
{
    return lightData.clusterOffsets[clusterIdx];
}

FUNC_QUALIFIER uint GetClusterLightCount(uint baseIndex)
{
    return lightData.clusterLightIndices[baseIndex];
}

FUNC_QUALIFIER uint FetchClusterLightIndex(uint baseIndex, uint i)
{
    return lightData.clusterLightIndices[baseIndex + 1 + i];
}

FUNC_QUALIFIER void StoreClusterLightOffsetAndCount(uint clusterIdx, uint baseIndex, uint visibleCount)
{
    lightData.clusterOffsets[clusterIdx] = baseIndex;
    lightData.clusterLightIndices[baseIndex] = visibleCount;
}

FUNC_QUALIFIER void StoreClusterLightIndex(uint baseIndex, uint i, uint lightIdx)
{
    lightData.clusterLightIndices[baseIndex + 1 + i] = lightIdx;
}

struct LightUniforms
{
    vec4 ClusterValues; // x=scale, y=bias, z=maxSlices, w=shadowsEnabled
    vec4 ClusterSize;   // x=dimX, y=dimY, z=dimZ, w=tileSizeX
};

layout(std140, set = TileArraySet, binding = GlobalLightDataUBOSlot) uniform LightUBO
{
    LightUniforms data;
}
lightUniforms;

// Helper functions for cluster access
FUNC_QUALIFIER uint getClusterIndex(vec3 viewPos, mat4 projMat)
{
    float viewZ = -viewPos.z;
    uint slice = uint(max(log(viewZ) * lightUniforms.data.ClusterValues.x + lightUniforms.data.ClusterValues.y, 0.0));
    slice = min(slice, uint(lightUniforms.data.ClusterValues.z) - 1);

    vec3 clusterSize = lightUniforms.data.ClusterSize.xyz;
    vec4 clipPos = projMat * vec4(viewPos, 1.0);
    vec3 ndc = clipPos.xyz / clipPos.w;
    ndc.y = -ndc.y;
    vec2 uv = ndc.xy * 0.5 + 0.5;

    uint x = uint(uv.x * clusterSize.x);
    uint y = uint(uv.y * clusterSize.y);
    uint z = slice;

    x = min(x, uint(clusterSize.x) - 1);
    y = min(y, uint(clusterSize.y) - 1);

    return x + y * uint(clusterSize.x) + z * uint(clusterSize.x) * uint(clusterSize.y);
}
#endif // SHADERS_CLUSTER_LIGHT_DATA_H
