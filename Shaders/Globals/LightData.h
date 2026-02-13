#include "BindingSlots.h"

struct Light
{
    vec4 position;
    vec4 direction;
    vec4 color;
    vec4 cutoff;
};

struct DirectionalLight
{
    vec4 direction;
    vec4 color;
};

layout(scalar, set = TileArraySet, binding = GlobalTileArraySSBOSlot) buffer GlobalTileSSBO
{
    DirectionalLight dirLight;
    uint numLights;
    uint _pad[3];
    Light lights[MAX_SCENE_LIGHTS];

    // Cluster light indices
    uint clusterOffsets[MAX_CLUSTERS + 1];
    uint clusterLightIndices[MAX_LIGHT_INDICES];
}
lightData;

struct LightUniforms
{
    vec4 CameraPos;
    vec4 LightGlobals;  // x = exposure, y = toneMapperType, z = ambientIntensity, w = csmDebugView
    vec4 ClusterValues; // x=scale, y=bias, z=maxSlices, w=shadowsEnabled
    vec4 ClusterSize;   // x=dimX, y=dimY, z=dimZ, w=tileSizeX
    vec4 GT7Params;     // x=PaperWhite, y=ReferenceLuminance, z=unused, w=unused
};
layout(std140, set = TileArraySet, binding = GlobalLightDataUBOSlot) uniform LightUBO
{
    LightUniforms data;
}
lightUniforms;

// Helper functions for cluster access
uint getClusterIndex(vec3 viewPos, mat4 projMat)
{
    float viewZ =
        -viewPos.z; 

    // Logarithmic Z slicing: index = log(z) * scale + bias
    uint slice = uint(max(log(viewZ) * lightUniforms.data.ClusterValues.x + lightUniforms.data.ClusterValues.y, 0.0));
    slice = min(slice, uint(lightUniforms.data.ClusterValues.z) - 1);

    vec3 clusterSize = lightUniforms.data.ClusterSize.xyz;

    vec4 clipPos = projMat * vec4(viewPos, 1.0);
    vec3 ndc = clipPos.xyz / clipPos.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;

    uint x = uint(uv.x * clusterSize.x);
    uint y = uint(uv.y * clusterSize.y);
    uint z = slice;

    x = min(x, uint(clusterSize.x) - 1);
    y = min(y, uint(clusterSize.y) - 1);

    return x + y * uint(clusterSize.x) + z * uint(clusterSize.x) * uint(clusterSize.y);
}