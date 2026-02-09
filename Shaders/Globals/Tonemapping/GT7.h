#include "GT7_Utils.h"

// GT7 functions based on the reference implementation:
// https://blog.selfshadow.com/publications/s2025-shading-course/pdi/supplemental/gt7_tone_mapping.cpp

vec3 GT7TMO(vec3 color)
{
    float paperWhite = lightUniforms.data.GT7Params.x; // e.g. 250.0
    float refLuminance = lightUniforms.data.GT7Params.y; // e.g. 100.0

    // If uniforms are 0 (uninitialized), set defaults
    if (paperWhite < 1.0) paperWhite = 250.0;
    if (refLuminance < 1.0) refLuminance = 100.0;
    
    float framebufferLuminanceTarget = physicalValueToFrameBufferValue(paperWhite, refLuminance);
    
    // Initialize curve parameters (GT7 sample defaults)
    // alpha=0.25, midPoint=0.538, linearSection=0.444, toeStrength=1.280
    GTToneMappingCurveParams curveParams = InitGTCurve(framebufferLuminanceTarget, 0.25, 0.538, 0.444, 1.280);

    // Default parameters from sample
    float blendRatio = 0.6;
    float fadeStart  = 0.98;
    float fadeEnd    = 1.16;
    
    // Calculate framebufferLuminanceTargetUcs
    vec3 targetRGB = vec3(framebufferLuminanceTarget);
    vec3 targetUCS;
    rgbToUcs(targetRGB, refLuminance, targetUCS);
    float framebufferLuminanceTargetUcs = targetUCS.x; // I component

    // Convert to UCS
    vec3 ucs;
    rgbToUcs(color, refLuminance, ucs);

    // Per-channel tone mapping ("skewed" color)
    vec3 skewedRgb = vec3(EvaluateGTCurve(color.r, curveParams),
                          EvaluateGTCurve(color.g, curveParams),
                          EvaluateGTCurve(color.b, curveParams));

    vec3 skewedUcs;
    rgbToUcs(skewedRgb, refLuminance, skewedUcs);

    float chromaScale = chromaCurve(ucs.x / framebufferLuminanceTargetUcs, fadeStart, fadeEnd);

    const vec3 scaledUcs = vec3(skewedUcs.x,         // Luminance from skewed color
                                ucs.y * chromaScale, // Scaled chroma components
                                ucs.z * chromaScale);

    // Convert back to RGB
    vec3 scaledRgb;
    ucsToRgb(scaledUcs, refLuminance, scaledRgb);

    // Final blend
    vec3 outColor;
    // Simple for loop unroll for vec3
    outColor.x = (1.0 - blendRatio) * skewedRgb.x + blendRatio * scaledRgb.x;
    outColor.y = (1.0 - blendRatio) * skewedRgb.y + blendRatio * scaledRgb.y;
    outColor.z = (1.0 - blendRatio) * skewedRgb.z + blendRatio * scaledRgb.z;
    
    // Apply SDR correction factor, assume we just output SDR for now always
    float correction = 1.0 / physicalValueToFrameBufferValue(paperWhite, refLuminance);
    
    outColor = min(outColor, vec3(framebufferLuminanceTarget));
    outColor *= correction;
    
    return outColor;
}
