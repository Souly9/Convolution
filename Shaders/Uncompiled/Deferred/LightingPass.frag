#version 450 core
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#define SharedDataUBOSet     1
#define TransformSSBOSet     2
#define TileArraySet         3
#define GBufferUBOSet        4
#define ShadowViewUBOSet     5
#define PassPerObjectDataSet 10

#include "../../Globals/Common.h"
#include "../../Globals/Bindless.h"
#include "../../Globals/Shadows/Data.h"
#include "../../Globals/GBuffer/GBufferSampling.h"
#include "../../Globals/Shadows/ShadowSampling.h"
#include "../../Globals/ClusterLightData.h"
#include "../../Globals/UnrealPBR.h"
#include "../../Globals/GeometryPassData.h"
#include "../../Globals/Tonemapping.h"
#include "../../Globals/Utils/Math.h"

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

float SampleGlobalShadowmaps(vec4 fragWorldPos, float viewDepth, vec3 normal, vec3 lightDir)
{
    float shadow = 1.0;
    if ((ubo.debugFlags & 1u) != 0u)
    {
        shadow = computeShadow(fragWorldPos, viewDepth, normal, lightDir);
    }
    return shadow;
}

void main()
{
    vec2 texCoords = IN.fragTexCoord;
    vec3 albedo = texture(GlobalBindlessTextures[nonuniformEXT(gbufferUBO.gbufferAlbedoIdx)], texCoords).xyz;
    vec4 gbufferTexCoordMat = texture(GlobalBindlessTextures[nonuniformEXT(gbufferUBO.gbufferTexCoordMatIdx)], texCoords);
    vec2 materialUV = gbufferTexCoordMat.xy;
    vec4 fragNormalData = texture(GlobalBindlessTextures[nonuniformEXT(gbufferUBO.gbufferNormalIdx)], texCoords);
    uint fragMatIdx = uint(fragNormalData.w);
    Material fragMaterial = globalObjectDataSSBO.materials[fragMatIdx];
    vec3 fragNormal = fragNormalData.xyz;

    float fragmentDepth = texture(GlobalBindlessTextures[nonuniformEXT(gbufferUBO.depthBufferIdx)], texCoords).r;

    vec4 fragPosWorldSpace = DepthToWorldSpace(texCoords, fragmentDepth, ubo.jitteredViewProjectionInverse);
    mat4 view = ubo.view;
    vec4 fragPosViewSpace = view * fragPosWorldSpace; // Technically already got it when calculating world from depth but this is fiiine

    vec3 normal = fragNormal;
    vec3 camPos = ubo.viewPos.xyz;

    vec3 viewDir = normalize(camPos - fragPosWorldSpace.xyz);
    int debugViewMode = ubo.debugViewMode;

    DirectionalLight dirLight = lightData.dirLight;
    vec3 dirLightTravelDir = normalize(dirLight.direction.xyz);
    float viewDepth = abs(fragPosViewSpace.z);
    vec3 N = normal;
    vec3 V = viewDir;

    // 1 = CSM Debug
    if (debugViewMode == 1)
    {
        int cascadeIdx = getCascadeIndex(viewDepth);
        vec3 cascadeColor = getCascadeDebugColor(cascadeIdx);

        float shadow = computeShadow(fragPosWorldSpace, viewDepth, N, dirLightTravelDir);
        vec3 debugColor = mix(cascadeColor, albedo, shadow);

        outColor.rgb = debugColor;
        outColor.a = 1.0;
        return;
    }
    // 2 = Cluster Debug
    else if (debugViewMode == 2)
    {
        uint clusterIdx = getClusterIndex(fragPosViewSpace.xyz, ubo.projection);
        float r = fract(sin(float(clusterIdx) * 12.9898 + 78.233) * 43758.5453);
        float g = fract(sin(float(clusterIdx) * 26.6518 + 12.453) * 21356.1235);
        float b = fract(sin(float(clusterIdx) * 98.3125 + 98.654) * 87541.2356);

        outColor.rgb = vec3(r, g, b);
        outColor.a = 1.0;
        return;
    }

    Material mat = fragMaterial;
    SurfaceParameters surface = GetDefaultSurface(albedo * mat.baseColor.rgb, mat.pbr1.y, mat.pbr1.x);
    
    surface.subsurface = mat.pbr1.z;
    surface.specular = mat.pbr1.w;

    surface.anisotropic = mat.pbr2.x;
    //surface.specularTint = mat.pbr2.y;
    surface.clearcoat = mat.pbr2.z;
    surface.clearcoatGloss = mat.pbr2.w;

    surface.sheen = mat.pbr3.x;
    //surface.sheenTint = mat.pbr3.y;
    //surface.specTrans = mat.pbr3.z;
    //surface.ior = mat.pbr3.w;

    if (IsMaterialFlagSet(mat.flags, MATERIAL_FLAG_METALLIC_ROUGHNESS_BIT))
    {
        vec3 sampledData = texture(GlobalBindlessTextures[nonuniformEXT(mat.metallicRoughnessTexture)], materialUV).rgb;
        surface.roughness *= sampledData.g;
        surface.metallic *= sampledData.b;
    }

    if (IsMaterialFlagSet(mat.flags, MATERIAL_FLAG_SHEEN_BIT))
    {
        surface.sheen *= texture(GlobalBindlessTextures[nonuniformEXT(mat.sheenTexture)], materialUV).r;
    }
    if (IsMaterialFlagSet(mat.flags, MATERIAL_FLAG_CLEARCOAT_BIT))
    {
        surface.clearcoat *= texture(GlobalBindlessTextures[nonuniformEXT(mat.clearcoatTexture)], materialUV).r;
    }
    surface = ApplyReflectanceDebug(surface, ubo.rtUseGlobalMaterialReflectance, ubo.rtGlobalMaterialReflectance);

    vec3 materialAlbedo = surface.baseColor;
    vec3 emissive = mat.emissive.rgb;
    if (IsMaterialFlagSet(mat.flags, MATERIAL_FLAG_EMISSIVE_BIT))
    {
        emissive *= texture(GlobalBindlessTextures[nonuniformEXT(mat.emissiveTexture)], materialUV).rgb;
    }
    vec3 directLighting = vec3(0.0);

    // Clustered Lighting logic
    uint clusterIdx = getClusterIndex(fragPosViewSpace.xyz, ubo.projection);

    // Read index from lightData (simplified for loop)
    uint baseIndex = lightData.clusterOffsets[clusterIdx];
    uint lightCount = lightData.clusterOffsets[clusterIdx + 1] - baseIndex;
    lightCount = min(lightCount, MAX_LIGHTS_PER_CLUSTER);

    for (uint i = 0; i < lightCount; ++i)
    {
        uint globalLightIdx = lightData.clusterLightIndices[baseIndex + i];
        Light light = lightData.lights[globalLightIdx];

        vec3 lightPos = light.position.xyz;
        float lightRange = light.direction.w;
        float lightIntensity = light.color.w;
        vec3 lightColor = light.color.xyz * lightIntensity;
        
        directLighting += computePointLight(
            fragPosWorldSpace.xyz, lightPos, lightRange, V, N, lightColor, surface);
    }

    // --- Directional Light ---
    float dirLightShadow = SampleGlobalShadowmaps(fragPosWorldSpace, viewDepth, N, dirLightTravelDir);
    if ((ubo.debugFlags & 2u) != 0u)
    {
        float sss = clamp(texture(GlobalBindlessTextures[nonuniformEXT(shadowmapViewUBO.screenSpaceShadows)], texCoords).r, 0.0, 1.0);
        float contactShadowStrength = 0.9;
        dirLightShadow *= mix(1.0, sss, contactShadowStrength);
    }
    {
        float lightIntensity = dirLight.color.w;
        vec3 lightColor = dirLight.color.xyz * lightIntensity;
        directLighting += computeDirLight(dirLightTravelDir, V, N, lightColor, surface) * dirLightShadow;
    }

    float ambientIntensity = ubo.ambientIntensity;
    vec3 indirectLighting = materialAlbedo * ambientIntensity;

    vec3 finalHDRColor = (directLighting + indirectLighting + emissive) * ubo.exposure;

    outColor = vec4(finalHDRColor, 1.0);
}

