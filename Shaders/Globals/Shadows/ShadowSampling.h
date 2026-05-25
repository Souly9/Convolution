#ifndef SHADERS_SHADOWS_SAMPLING_H
#define SHADERS_SHADOWS_SAMPLING_H

#include "../Common.h"
#include "Data.h"
#include "../GBuffer/GBufferSampling.h"
#include "../Bindless.h"

float getCascadeSplit(int index)
{
    int arrayIdx = index / 4;
    int compIdx = index % 4;
    return shadowmapViewUBO.cascadeSplits[arrayIdx][compIdx];
}

int getCascadeIndex(float viewDepth)
{
    int cascadeCount = shadowmapViewUBO.cascadeCount;

    for (int j = 0; j < cascadeCount; ++j)
    {
        float splitDist = getCascadeSplit(j);
        if (viewDepth < splitDist)
        {
            return j;
        }
    }

    return cascadeCount - 1;
}

const vec2 poissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216), vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870), vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432), vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845), vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554), vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023), vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507), vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367), vec2(0.14383161, -0.14100790)
);

float sampleShadowCascade(int cascadeIndex, vec4 fragWorldPos, vec3 normal, vec3 lightDir)
{
    vec4 fragPosLightSpace = shadowmapViewUBO.csmViewMatrices[cascadeIndex] * fragWorldPos;
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    projCoords.xy = vec2(projCoords.x * 0.5 + 0.5, 0.5 - projCoords.y * 0.5);

    if (projCoords.z > 1.0 || projCoords.z < 0.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 ||
        projCoords.y > 1.0)
    {
        return 1.0;
    }

    vec3 lightToSurfaceDir = normalize(-lightDir);
    float cosTheta = clamp(dot(normalize(normal), lightToSurfaceDir), 0.0, 1.0);

    ivec3 shadowMapSize = textureSize(GlobalBindlessArrayTextures[shadowmapUBO.directionalShadowMapIdx], 0);
    float shadowMapWidth = float(max(shadowMapSize.x, 1));
    // CSM projections use a 2r-wide XY extent and a 9r depth extent, so this converts texels to normalized depth.
    float normalizedDepthPerTexel = 2.0 / (9.0 * shadowMapWidth);
    float biasTexels = mix(0.5, 2.0, 1.0 - cosTheta);
    float bias = biasTexels * normalizedDepthPerTexel;

    float currentDepth = (projCoords.z + bias);
    float shadow = 0.0;
    float baseRadius = 0.0005;
    float scale = exp2(-float(cascadeIndex));
    float diskRadius = baseRadius * scale;
    int samples = 16;
    for (int i = 0; i < samples; i++)
    {
        float pcfDepth = texture(GlobalBindlessArrayTextures[shadowmapUBO.directionalShadowMapIdx],
                                 vec3(projCoords.xy + poissonDisk[i] * diskRadius, cascadeIndex))
                             .x;
        shadow += (currentDepth < pcfDepth ? 0.0 : 1.0);
    }

    shadow /= float(samples);
    return shadow;
}

float computeShadow(vec4 fragWorldPos, float fragViewDepth, vec3 normal, vec3 lightDir)
{

    int cascadeIndex = getCascadeIndex(fragViewDepth);
    int cascadeCount = shadowmapViewUBO.cascadeCount;
    if (cascadeIndex < 0 || cascadeIndex >= cascadeCount)
        return 1.0;

    float shadow = sampleShadowCascade(cascadeIndex, fragWorldPos, normal, lightDir);

    // Fade between cascades
    int nextCascade = cascadeIndex + 1;
    if (nextCascade < shadowmapViewUBO.cascadeCount)
    {
        float currentSplit = getCascadeSplit(cascadeIndex);
        float nextSplit = getCascadeSplit(nextCascade);

        float transitionRange = 0.5;
        float distanceToEdge = currentSplit - fragViewDepth;

        if (distanceToEdge < transitionRange)
        {
            float nextShadow = sampleShadowCascade(nextCascade, fragWorldPos, normal, lightDir);
            float t = 1.0 - (distanceToEdge / transitionRange);
            shadow = mix(shadow, nextShadow, smoothstep(0.0, 1.0, t));
        }
    }

    return shadow;
}

#endif // SHADERS_SHADOWS_SAMPLING_H




