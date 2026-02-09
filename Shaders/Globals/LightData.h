#include "BindingSlots.h"

// Match C++ Define
#define MAX_SCENE_LIGHTS       1024
#define MAX_CLUSTERS           (16 * 32 * 64)
#define MAX_LIGHT_INDICES      (MAX_CLUSTERS * 128)
#define MAX_LIGHTS_PER_CLUSTER 64

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
        -viewPos.z; // View space Z is negative forward in Vulkan/GLM usually, if viewPos is from standard View matrix.

    // Linear Z slicing: index = z * scale + bias
    uint slice = uint(max(viewZ * lightUniforms.data.ClusterValues.x + lightUniforms.data.ClusterValues.y, 0.0));
    slice = min(slice, uint(lightUniforms.data.ClusterValues.z) - 1);

    vec3 clusterSize = lightUniforms.data.ClusterSize.xyz;

    vec4 clipPos = projMat * vec4(viewPos, 1.0);
    vec3 ndc = clipPos.xyz / clipPos.w;
    vec2 uv = ndc.xy * 0.5 + 0.5;

    // Normalize UV 0..1 (Top-Left 0,0 for Vulkan?)
    // Vulkan NDC Y is down?
    // If standard GL logic `uv = ndc * 0.5 + 0.5`, Y is up.
    // Vulkan has Y down in CLIP space?
    // If we rely on standard UV mapping logic:
    // If fragment shader inputs `texCoords` are 0..1, we can use them if we had them.
    // But we are computing from ViewPos.
    // We should assume `uv` computed here matches the screen UV.

    uint x = uint(uv.x * clusterSize.x);
    uint y = uint(uv.y * clusterSize.y);
    uint z = slice;

    x = min(x, uint(clusterSize.x) - 1);
    y = min(y, uint(clusterSize.y) - 1);

    return x + y * uint(clusterSize.x) + z * uint(clusterSize.x) * uint(clusterSize.y);
}