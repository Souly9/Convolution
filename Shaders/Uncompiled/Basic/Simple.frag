#version 450 core
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#include "../../Globals/LightingPass.h"

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

float SampleGlobalShadowmaps(vec4 worldPos, float viewDepth, vec3 normal, vec3 lightDir)
{
    float shadow = 1.0;
    if (IsFlagSet(ubo.debugFlags, DEBUG_FLAG_SHADOWS_ENABLED))
    {
        shadow = computeShadow(worldPos, viewDepth, normal, lightDir);
    }
    return shadow;
}

void main()
{
    vec2 texCoords = IN.fragTexCoord;
    vec3 albedo = texture(GlobalBindlessTextures[gbufferUBO.gbufferAlbedoIdx], texCoords).xyz;
    vec4 gbufferTexCoordMat = texture(GlobalBindlessTextures[gbufferUBO.gbufferTexCoordMatIdx], texCoords);
    vec2 materialUV = gbufferTexCoordMat.xy;
    vec4 fragNormalData = texture(GlobalBindlessTextures[gbufferUBO.gbufferNormalIdx], texCoords);
    uint fragMatIdx = uint(fragNormalData.w);
    Material fragMaterial = globalObjectDataSSBO.materials[fragMatIdx];
    vec3 fragNormal = fragNormalData.xyz;

    float fragmentDepth = texture(GlobalBindlessTextures[gbufferUBO.depthBufferIdx], texCoords).r;

    vec4 clipSpacePosition;
    clipSpacePosition.x = texCoords.x * 2.0 - 1.0;
    clipSpacePosition.y = (1.0 - texCoords.y) * 2.0 - 1.0; // Vulkan has flipped viewport here
    clipSpacePosition.z = fragmentDepth;                   
    clipSpacePosition.w = 1.0;
    clipSpacePosition = ubo.projectionInverse * clipSpacePosition;
    clipSpacePosition = (ubo.viewInverse * clipSpacePosition);

    clipSpacePosition.xyz /= clipSpacePosition.w;

    vec4 fragPosWorldSpace = vec4(clipSpacePosition.xyz, 1);
    mat4 view = ubo.view;
    vec4 fragPosViewSpace = view * fragPosWorldSpace;

    vec3 normal = fragNormal;
    vec3 camPos = ubo.viewPos.xyz;

    vec3 viewDir = normalize(camPos - fragPosWorldSpace.xyz);
    // Debug View modes

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

        // Use a simple hash to generate a color from the cluster index
        float r = fract(sin(float(clusterIdx) * 12.9898 + 78.233) * 43758.5453);
        float g = fract(sin(float(clusterIdx) * 26.6518 + 12.453) * 21356.1235);
        float b = fract(sin(float(clusterIdx) * 98.3125 + 98.654) * 87541.2356);

        vec3 debugColor = vec3(r, g, b);

        outColor.rgb = debugColor;
        outColor.a = 1.0;
        return;
    }

    Material mat = fragMaterial;
    SurfaceParameters surface = GetDefaultSurface(albedo * mat.baseColor.rgb, mat.roughness.x, mat.metallic.x);
    
    surface.anisotropic = mat.anisotropicTintIorClearcoat.x;
    surface.specularTint = mat.anisotropicTintIorClearcoat.y;
    surface.ior = mat.anisotropicTintIorClearcoat.z;
    surface.clearcoat = mat.anisotropicTintIorClearcoat.w;
    
    surface.clearcoatGloss = mat.glossSheenSTransFlatness.x;
    surface.sheen = mat.glossSheenSTransFlatness.y;
    surface.sheenTint = mat.glossSheenSTransFlatness.z;
    surface.specTrans = mat.glossSheenSTransFlatness.w;

    if (IsMaterialFlagSet(mat.flags, MATERIAL_FLAG_SHEEN_BIT))
    {
        surface.sheen *= texture(GlobalBindlessTextures[nonuniformEXT(mat.sheenTexture)], materialUV).r;
    }
    if (IsMaterialFlagSet(mat.flags, MATERIAL_FLAG_CLEARCOAT_BIT))
    {
        surface.clearcoat *= texture(GlobalBindlessTextures[nonuniformEXT(mat.clearcoatTexture)], materialUV).r;
    }

    vec3 materialAlbedo = surface.baseColor;
    vec3 emissive = mat.emissive.rgb;

    // (Specular + Diffuse)
    vec3 directLighting = vec3(0.0);

    // Clustered Lighting logic
    uint clusterIdx = getClusterIndex(fragPosViewSpace.xyz, ubo.projection);

    // Read start index from offsets buffer (written by compute shader)
    uint baseIndex = GetClusterLightBaseIndex(clusterIdx);
    uint lightCount = GetClusterLightCount(baseIndex);
    lightCount = min(lightCount, MAX_LIGHTS_PER_CLUSTER);

    // --- Point/Spot Light Loop ---
    for (uint i = 0; i < lightCount; ++i)
    {
        Light light = lightData.lights[FetchClusterLightIndex(baseIndex, i)];

        vec3 lightPos = light.position.xyz;
        float lightRange = light.direction.w;
        float lightIntensity = light.color.w;
        vec3 lightColor = light.color.xyz * lightIntensity;
        

        vec3 lightContribution = computePointLight(
            fragPosWorldSpace.xyz, lightPos, lightRange, V, N, lightColor, surface);


        directLighting += lightContribution;
    }

    // --- Directional Light ---
    float dirLightShadow = SampleGlobalShadowmaps(fragPosWorldSpace, viewDepth, N, dirLightTravelDir);
    if (IsFlagSet(ubo.debugFlags, DEBUG_FLAG_SSS_ENABLED))
    {
        float sss = clamp(texture(GlobalBindlessTextures[shadowmapViewUBO.screenSpaceShadows], texCoords).r, 0.0, 1.0);
        float contactShadowStrength = 0.9;
        dirLightShadow *= mix(1.0, sss, contactShadowStrength);
    }
    {
        float lightIntensity = dirLight.color.w;
        vec3 lightColor = dirLight.color.xyz * lightIntensity;

        vec3 lightContribution = computeDirLight(dirLightTravelDir, V, N, lightColor, surface);

        directLighting += lightContribution * dirLightShadow; // Apply shadow only to directional light
    }

    float ambientIntensity = ubo.ambientIntensity;
    vec3 indirectLighting = computeAmbient(materialAlbedo, ambientIntensity);

    // Exposure
    float exposure = ubo.exposure;
    vec3 finalHDRColor = (directLighting + indirectLighting + emissive) * exposure;

    // Tone Mapping
    int toneMapperType = ubo.toneMapperType;
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
    // Apply UI Color overlay / Final output assignment
    vec4 debugColor = texture(GlobalBindlessTextures[gbufferUBO.gbufferDebugIdx], texCoords);

    // Convert Linear SDR Tonemapped output to sRGB SDR presentation output
    vec3 finalSceneColor = finalLDRColor;
    // finalSceneColor += debugColor.rgb;
    finalSceneColor = pow(finalSceneColor, vec3(1.0 /2.2));

    outColor.rgb = finalSceneColor;
    outColor.a = 1.0;
}
