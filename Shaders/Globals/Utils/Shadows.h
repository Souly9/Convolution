
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

vec2 poissonDisk[16] = vec2[]( 
   vec2( -0.94201624, -0.39906216 ), 
   vec2( 0.94558609, -0.76890725 ), 
   vec2( -0.094184101, -0.92938870 ), 
   vec2( 0.34495938, 0.29387760 ), 
   vec2( -0.91588581, 0.45771432 ), 
   vec2( -0.81544232, -0.87912464 ), 
   vec2( -0.38277543, 0.27676845 ), 
   vec2( 0.97484398, 0.75648379 ), 
   vec2( 0.44323325, -0.97511554 ), 
   vec2( 0.53742981, -0.47373420 ), 
   vec2( -0.26496911, -0.41893023 ), 
   vec2( 0.79197514, 0.19090188 ), 
   vec2( -0.24188840, 0.99706507 ), 
   vec2( -0.81409955, 0.91437590 ), 
   vec2( 0.19984126, 0.78641367 ), 
   vec2( 0.14383161, -0.14100790 ) 
);  

float sampleShadowCascade(int cascadeIndex, vec4 fragPosWorldSpace, vec3 normal, vec3 lightDir)
{
    vec4 fragPosLightSpace = shadowmapViewUBO.csmViewMatrices[cascadeIndex] * fragPosWorldSpace;
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    
    projCoords.x = projCoords.x * 0.5 + 0.5;
    projCoords.y = 1.0 - (projCoords.y * 0.5 + 0.5);

    if (projCoords.z > 1.0 || projCoords.z < 0.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0)
    {
        return 0.0;
    }
    // Bias logic
    float cosTheta = clamp(dot(normal, lightDir), 0.0, 1.0);
    float bias = max(0.005 * (1.0 - cosTheta), 0.0005);
    
    float currentDepth = (projCoords.z - bias);
    float shadow = 0.0;
    float diskRadius = 0.0005; 
    int samples = 16;
    for (int i = 0; i < samples; i++)
    {
        float pcfDepth = texture(GlobalBindlessArrayTextures[shadowmapUBO.directionalShadowMapIdx],
                                 vec3(projCoords.xy + poissonDisk[i] * diskRadius, cascadeIndex)).x;
        shadow += (currentDepth > (pcfDepth) ? 0.0 : 1.0);
    }
    
    shadow /= float(samples);
    return shadow;
}

float computeShadow(vec4 fragPosWorldSpace, float viewDepth, vec3 normal, vec3 lightDir)
{
    // Check if shadows are globally disabled
    if (lightUniforms.data.ClusterValues.w < 0.5)
        return 1.0;

    int cascadeIndex = getCascadeIndex(viewDepth);
    int cascadeCount = shadowmapViewUBO.cascadeCount;
    if (cascadeIndex < 0 || cascadeIndex >= cascadeCount)
        return 0.0;

    float shadow = sampleShadowCascade(cascadeIndex, fragPosWorldSpace, normal, lightDir);
    
    // Fade between cascades
    int nextCascade = cascadeIndex + 1;
    if (nextCascade < shadowmapViewUBO.cascadeCount)
    {
        float currentSplit = getCascadeSplit(cascadeIndex);
        float nextSplit = getCascadeSplit(nextCascade);
        
        float transitionRange = (nextSplit - currentSplit) * 0.05;
        float distanceToEdge = currentSplit - viewDepth;

        if (distanceToEdge < transitionRange)
        {
            float nextShadow = sampleShadowCascade(nextCascade, fragPosWorldSpace, normal, lightDir);
            float t = 1.0 - (distanceToEdge / transitionRange);
            shadow = mix(shadow, nextShadow, smoothstep(0.0, 1.0, t));
        }
    }

    return shadow;
}
