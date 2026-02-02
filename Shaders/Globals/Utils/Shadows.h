// Returns cascade index based on view-space depth
int getCascadeIndex(vec4 fragPosViewSpace)
{
    float depthValue = abs(fragPosViewSpace.z);
    int cascadeCount = shadowmapViewUBO.cascadeCount;

    int cascadeIndex = cascadeCount - 1;
    for (int i = 0; i < cascadeCount - 1; ++i)
    {
        if (depthValue < shadowmapViewUBO.cascadeSplits[i / 4][i % 4])
        {
            cascadeIndex = i;
            break;
        }
    }
    return cascadeIndex;
}

float computeShadow(vec4 fragPosViewSpace, vec4 fragPosWorldSpace, vec3 fragNormal, vec3 lightDir)
{
    // Check if shadows are globally disabled
    if (lightUniforms.data.ClusterValues.w < 0.5)
        return 1.0;

    // 1. Cascade Selection
    int cascadeIndex = getCascadeIndex(fragPosViewSpace);

    // 2. Project to Light Space
    vec4 fragPosLightSpace = shadowmapViewUBO.csmViewMatrices[cascadeIndex] * fragPosWorldSpace;
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // 3. Transform to [0,1] range
    // NOTE: If using standard Vulkan clip space (Y-down), ensure your matrix handles the flip,
    // otherwise manual flip: projCoords.y = -projCoords.y; might be needed here.
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    // 4. Boundary Checks
    if (projCoords.z > 1.0 || projCoords.z < 0.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 ||
        projCoords.y > 1.0)
    {
        return 1.0;
    }

    // 5. Bias Calculation
    float nDotL = max(dot(normalize(fragNormal), lightDir), 0.0);
    float bias = max(0.005 * (1.0 - nDotL), 0.001);

    // 6. PCF Sampling
    float shadow = 0.0;
    float currentDepth = projCoords.z;

    vec2 texelSize = 1.0 / vec2(textureSize(GlobalBindlessArrayTextures[shadowmapUBO.directionalShadowMapIdx], 0).xy);

    // 3x3 Loop
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            vec3 coords = vec3(projCoords.xy + vec2(x, y) * texelSize, float(cascadeIndex));
            float pcfDepth = texture(GlobalBindlessArrayTextures[shadowmapUBO.directionalShadowMapIdx], coords).r;

            // Check shadow
            shadow += (currentDepth - bias) > pcfDepth ? 0.0 : 1.0;
        }
    }

    return shadow / 9.0;
}