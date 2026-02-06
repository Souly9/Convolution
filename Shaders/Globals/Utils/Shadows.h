// Returns cascade index based on view-space depth
int getCascadeIndex(float viewDepth)
{
    // Compare with split planes
    int cascadeCount = shadowmapViewUBO.cascadeCount;
    for (int i = 0; i < cascadeCount; ++i)
    {
        if (viewDepth < shadowmapViewUBO.cascadeSplits[i].x)
            return i;
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


float computeShadow(vec4 fragPosWorldSpace, float viewDepth, vec3 normal, vec3 lightDir)
{
    // Check if shadows are globally disabled
    if (lightUniforms.data.ClusterValues.w < 0.5)
        return 1.0;

    // 1. Get Cascade Index
    int cascadeIndex = getCascadeIndex(viewDepth);
    
    // 2. Project to Light Space of that cascade
    vec4 fragPosLightSpace = shadowmapViewUBO.csmViewMatrices[cascadeIndex] * fragPosWorldSpace;
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // 3. Transform to [0,1] range
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    // Early exit if out of bounds (though usually covered by cascade selection, safe to keep)
    if (projCoords.z > 1.0 || projCoords.z < 0.0 || 
        projCoords.x > 1.0 || projCoords.x < 0.0 || 
        projCoords.y > 1.0 || projCoords.y < 0.0)    
        return 1.0;

    // 4. Slope-Scaled Bias
    // dot(normal, lightDir) gives cosTheta. 
    // note: lightDir should be direction TO light (normalized).
    float cosTheta = clamp(dot(normal, lightDir), 0.0, 1.0);
    // Bias increases as surface gets steeper relative to light
    // Adjust bias for cascade level? Usually further cascades need less bias precision or just bigger bias?
    // Let's keep it simple.
    float bias = max(0.005 * (1.0 - cosTheta), 0.0005);
    
    // Scale bias with cascade index roughly? 
    // float biasScale = 1.0 / (shadowmapViewUBO.cascadeSplits[cascadeIndex].x * 0.5); // Conceptually 
    // simple constant bias is often enough if resolution is high.
    
    // 5. Poisson Disk Sampling (PCF)
    float currentDepth = projCoords.z - bias;
    float shadow = 0.0;
    
    // PCF Radius
    // Scale radius by cascade to keep consistent softness in world space?
    // Or constant in screen space?
    // Constant in texture space means constant world softness if ortho size matches.
    float diskRadius = 0.0005; 

    // Use loop for PCF
    for (int i = 0; i < 16; i++)
    {
        // Sample from the shadow map array
        float pcfDepth = texture(GlobalBindlessArrayTextures[shadowmapUBO.directionalShadowMapIdx], 
                                 vec3(projCoords.xy + poissonDisk[i] * diskRadius, cascadeIndex)).x; 
        
        shadow += (currentDepth > pcfDepth ? 0.0 : 1.0);
    }
    
    shadow /= 16.0;

    return shadow;
}
// Wait, replacing the whole file content is easier to just get right first time.
// I'll assume I update signature.
