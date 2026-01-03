
float computeShadow(vec4 fragPosViewSpace, vec4 fragPosWorldSpace, vec3 fragNormal, vec3 lightDir)
{
    float depthValue = abs(fragPosViewSpace.z);

    int cascadeIndex = 0;
    int cascadeCount = 3;
    float cascadeStepSize = shadowmapViewUBO.cascadeStepSize;
    for (int i = 0; i < cascadeCount - 1; ++i)
    {
        if (depthValue < (cascadeStepSize + (cascadeStepSize * i)))
        {
            cascadeIndex = i;
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
        return 0.0; 
    }
    vec2 uv = projCoords.xy * 0.5 + 0.5; 
    float currentDepth = projCoords.z; 
    vec3 coords = vec3(uv, cascadeIndex);
    float closestDepth = texture(
        GlobalBindlessArrayTextures[shadowmapUBO.directionalShadowMapIdx],
        coords
    ).r;

    // Apply bias to prevent shadow acne
    float bias = max(0.005 * (1.0 - dot(normalize(fragNormal), lightDir)), 0.0005);

    float shadow = (currentDepth - bias) > closestDepth ? 0.0 : 1.0; 

    return shadow;

}