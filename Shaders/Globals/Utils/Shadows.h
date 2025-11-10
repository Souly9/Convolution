
float computeShadow(vec4 fragPosViewSpace, vec4 fragPosWorldSpace, vec3 fragNormal, vec3 lightDir)
{
    float depthValue = abs(fragPosViewSpace.z);

    int cascadeIndex = 0;
    int cascadeCount = 3;
    float cascadeStepSize = shadowmapViewUBO.cascadeStepSize;
    for (int i = 0; i < cascadeCount - 1; ++i)
    {
        if (depthValue < cascadeStepSize + (cascadeStepSize * i))
        {
            cascadeIndex = i + 1;
            break;
        }
    }
    vec4 fragPosLightSpace = shadowmapViewUBO.csmViewMatrices[cascadeIndex] * fragPosWorldSpace;
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    if (projCoords.z > 1.0)
    {
        return 1.0; // Not in shadow
    }
    
    vec3 coords = vec3(projCoords.xy, cascadeIndex);
    float closestDepth = texture(
        GlobalBindlessArrayTextures[shadowmapUBO.directionalShadowMapIdx],
        coords
    ).r;
    float currentDepth = projCoords.z;

    // Apply bias to prevent shadow acne
    float bias = max(0.005 * (1.0 - dot(normalize(fragNormal), lightDir)), 0.0005);

    float shadow = (currentDepth - bias) > closestDepth ? 0.0 : 1.0; 

    return shadow;

}