#version 450 core
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#define ViewUBOSet           1
#define TransformSSBOSet     2
#define TileArraySet         3
#define GBufferUBOSet        4
#define ShadowViewUBOSet     5
#define PassPerObjectDataSet 10
#include "../../Globals/GBufferUBO.h"
#include "../../Globals/GlobalBuffers.h"
#include "../../Globals/LightData.h"
#include "../../Globals/PBR/UnrealPBR.h"
#include "../../Globals/ShadowBuffers.h"
#include "../../Globals/Textures.h"
#include "../../Globals/Utils/Shadows.h"

layout(location = 0) in VertexOut
{
    vec2 fragTexCoord;
}
IN;

layout(location = 0) out vec4 outColor;

// CSM debug colors for each cascade layer
vec3 getCascadeDebugColor(int cascadeIndex)
{
    const vec3 cascadeColors[8] = vec3[8](vec3(1.0, 0.2, 0.2), // Red - cascade 0 (closest)
                                          vec3(0.2, 1.0, 0.2), // Green - cascade 1
                                          vec3(0.2, 0.2, 1.0), // Blue - cascade 2
                                          vec3(1.0, 1.0, 0.2), // Yellow - cascade 3
                                          vec3(1.0, 0.2, 1.0), // Magenta - cascade 4
                                          vec3(0.2, 1.0, 1.0), // Cyan - cascade 5
                                          vec3(1.0, 0.6, 0.2), // Orange - cascade 6
                                          vec3(0.6, 0.2, 1.0)  // Purple - cascade 7
    );
    return cascadeColors[clamp(cascadeIndex, 0, 7)];
}

void main()
{
    vec2 texCoords = vec2(1.0 - IN.fragTexCoord.x, 1.0 - IN.fragTexCoord.y);
    vec3 albedo = texture(GlobalBindlessTextures[gbufferUBO.gbufferAlbedoIdx], texCoords).xyz;
    vec4 texCoordMatData = vec4(texture(GlobalBindlessTextures[gbufferUBO.gbufferTexCoordMatIdx], texCoords));
    uint fragMatIdx = uint(texCoordMatData.z);
    Material fragMaterial = globalObjectDataSSBO.materials[fragMatIdx];
    // vec4 fragTexSample = vec4(texture(GlobalBindlessTextures[6], fragTexCoords));
    // vec4 texColor = vec4(fragTexSample.xyz, 1);
    vec4 uiColor = vec4(texture(GlobalBindlessTextures[gbufferUBO.gbufferUIIdx], texCoords));
    uiColor.a = mix(0, 0.8, uiColor.a);

    vec4 fragPosWorldSpace = vec4(texture(GlobalBindlessTextures[gbufferUBO.gbufferPositionIdx], texCoords).xyz, 1);
    vec3 fragNormal = texture(GlobalBindlessTextures[gbufferUBO.gbufferNormalIdx], texCoords).xyz;

    mat4 view = ubo.view;
    vec4 fragPosViewSpace = view * fragPosWorldSpace;

    // Debug View modes
    
    int debugViewMode = int(lightUniforms.data.LightGlobals.w);

    DirectionalLight dirLight = lightData.dirLight;
    vec3 L = normalize(dirLight.direction.xyz);
    // 1 = CSM Debug
    if (debugViewMode == 1)
    {
        float viewDepth = abs(fragPosViewSpace.z);
        int cascadeIdx = getCascadeIndex(viewDepth);
        vec3 cascadeColor = getCascadeDebugColor(cascadeIdx);
        
        // Calculate shadow factor for this fragment
        float shadow = computeShadow(fragPosWorldSpace, viewDepth, fragNormal, L);
        
        // "Shadowed parts are colored in different colors"
        // If shadow is 0 (shadowed), show cascadeColor.
        // If shadow is 1 (lit), show Albedo (or similar).
        
        // Mix: 0.0 (shadow) -> cascadeColor, 1.0 (lit) -> albedo
        vec3 debugColor = mix(cascadeColor, albedo, shadow);
        
        outColor.rgb = debugColor * (1.0 - uiColor.a) + uiColor.rgb * uiColor.a;
        outColor.a = 1.0;
        return;
    }
    // 2 = Cluster Debug
    else if (debugViewMode == 2)
    {
        uint clusterIdx = getClusterIndex(fragPosViewSpace.xyz, ubo.proj);

        // Use a simple hash to generate a color from the cluster index
        float r = fract(sin(float(clusterIdx) * 12.9898 + 78.233) * 43758.5453);
        float g = fract(sin(float(clusterIdx) * 26.6518 + 12.453) * 21356.1235);
        float b = fract(sin(float(clusterIdx) * 98.3125 + 98.654) * 87541.2356);

        vec3 debugColor = vec3(r, g, b);

        outColor.rgb = debugColor * (1.0 - uiColor.a) + uiColor.rgb * uiColor.a;
        outColor.a = 1.0;
        return;
    }

    vec3 normal = fragNormal;
    vec3 worldPos = fragPosWorldSpace.xyz;
    vec3 camPos = lightUniforms.data.CameraPos.xyz;

    Material mat = fragMaterial;

    // Use material roughness or custom set roughness
    float roughness = 1;
    float metallic = 0;

    vec3 viewDir = normalize(camPos - worldPos.xyz);

    vec3 N = normalize(normal);
    vec3 V = viewDir;
    // (Specular + Diffuse)
    vec3 directLighting = vec3(0.0);

    // Clustered Lighting logic
    uint clusterIdx = getClusterIndex(fragPosViewSpace.xyz, ubo.proj);

    // Read start index from offsets buffer (written by compute shader)
    uint baseIndex = lightData.clusterOffsets[clusterIdx];
    uint lightCount = lightData.clusterLightIndices[baseIndex];

    // --- Point/Spot Light Loop ---
    for (uint i = 0; i < lightCount; ++i)
    {
        Light light = lightData.lights[lightData.clusterLightIndices[baseIndex + i]];

        vec3 lightPos = light.position.xyz;
        float lightIntensity = light.color.w;
        vec3 lightColor = light.color.xyz * lightIntensity;
        // Direction and other params for spot lights would be accessed here if needed
        // For now treating as point lights or checking type

        vec3 lightContribution =
            computePointLight(worldPos.xyz, lightPos, V, N, lightColor, albedo, roughness, metallic);

        // Apply shadow factor and accumulate
        directLighting += lightContribution;
    }

    // --- Directional Light ---
    {
        vec3 lightDir = normalize(dirLight.direction.xyz);
        float lightIntensity = dirLight.color.w;
        vec3 lightColor = dirLight.color.xyz * lightIntensity;


        vec3 lightContribution = computeDirLight(lightDir, V, N, lightColor, albedo, roughness, metallic);

        directLighting += lightContribution;
    }
    // Update call to computeShadow with viewDepth
    float viewDepth = abs(fragPosViewSpace.z);
    float dirLightShadow = computeShadow(fragPosWorldSpace, viewDepth, N, normalize(dirLight.direction.xyz));

    float ambientIntensity = lightUniforms.data.LightGlobals.z;
    vec3 indirectLighting = computeAmbient(albedo, ambientIntensity);

    // Exposure
    float exposure = lightUniforms.data.LightGlobals.x;
    vec3 finalHDRColor = (directLighting * dirLightShadow + indirectLighting) * exposure;

    // Tone Mapping
    int toneMapperType = int(lightUniforms.data.LightGlobals.y);
    vec3 finalLDRColor = finalHDRColor;

    if (toneMapperType == 1) // ACES
    {
        finalLDRColor = AcesTMO(finalHDRColor);
    }
    else if (toneMapperType == 2) // Uncharted
    {
        finalLDRColor = Uncharted2TMO(finalHDRColor);
    }
    else if (toneMapperType == 3) // GT7
    {
        finalLDRColor = GT7TMO(finalHDRColor);
    }
    // 0 = None (Linear code will pass through) purely clamped at output if swapchain is integer,
    // but we use float swapchain so it allows HDR output.

    // Apply UI Color overlay / Final output assignment
    outColor.rgb = finalLDRColor * (1.0 - uiColor.a) + uiColor.rgb * uiColor.a;
    outColor.a = 1.0;
}
