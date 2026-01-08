#version 450 core
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
#define ViewUBOSet           1
#define TransformSSBOSet     2
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

    Light lights[MAX_LIGHTS_PER_TILE] = lightTileArraySSBO.tiles[0].lights;
    int lightCount = lightTileArraySSBO.tiles.length();
    // --- Point Light Loop ---
    for (int i = 0; i < lightCount; ++i)
    {
        Light light = lights[i];
        vec3 lightPos = light.position.xyz;
        vec3 lightColor = light.color.xyz;

        vec3 lightContribution =
            computePointLight(worldPos.xyz, lightPos, V, N, lightColor, albedo, roughness, metallic);

        // Apply shadow factor and accumulate
        directLighting += lightContribution;
    }

    // --- Directional Light ---
    {
        DirectionalLight dirLight = lightTileArraySSBO.dirLight;
        vec3 lightDir = normalize(dirLight.direction.xyz);
        vec3 lightColor = dirLight.color.xyz;

        float dirLightShadow = computeShadow(fragPosViewSpace, fragPosWorldSpace, fragNormal, lightDir);

        vec3 lightContribution = computeDirLight(lightDir, V, N, lightColor, albedo, roughness, metallic);

        directLighting += lightContribution * dirLightShadow;
    }

    vec3 indirectLighting = computeAmbient(albedo);

    vec3 finalHDRColor = directLighting + indirectLighting;

    // Tone Mapping
    vec3 finalLDRColor = AcesTMO(finalHDRColor);

    // Apply UI Color overlay / Final output assignment
    outColor.rgb = finalLDRColor * (1.0 - uiColor.a) + uiColor.rgb * uiColor.a;
    outColor.a = 1.0;
}
