#ifndef TONEMAPPING_H
#define TONEMAPPING_H

FUNC_QUALIFIER vec3 AcesTMO(vec3 color)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0f, 1.0f);
}

FUNC_QUALIFIER vec3 Uncharted2TMO(vec3 color)
{
    const float A = 0.15f;
    const float B = 0.50f;
    const float C = 0.10f;
    const float D = 0.20f;
    const float E = 0.02f;
    const float F = 0.30f;
    const float W = 11.2f;

    vec3 x = color;
    vec3 curr = ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
    
    x = vec3(W);
    vec3 whiteScale = 1.0f / (((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F);
    return curr * whiteScale;
}

// --- GT7 Tonemapping Logic (Merged from GT7.h & GT7_Utils.h) ---

#define GRAN_TURISMO_SDR_PAPER_WHITE 250.0 // cd/m^2
#define REFERENCE_LUMINANCE 100.0 // cd/m^2 <-> 1.0f

FUNC_QUALIFIER float physicalValueToFrameBufferValue(float physical, float refLum)
{
    return physical / refLum;
}

FUNC_QUALIFIER float frameBufferValueToPhysicalValue(float fbValue, float refLum)
{
    return fbValue * refLum;
}

struct GTToneMappingCurveParams
{
    float peakIntensity;
    float alpha;
    float midPoint;
    float linearSection;
    float toeStrength;
    float kA, kB, kC;
};

FUNC_QUALIFIER GTToneMappingCurveParams InitGTCurve(float monitorIntensity, float alpha, float midPoint, float linearSection, float toeStrength)
{
    GTToneMappingCurveParams params;
    params.peakIntensity = monitorIntensity;
    params.alpha = alpha;
    params.midPoint = midPoint;
    params.linearSection = linearSection;
    params.toeStrength = toeStrength;

    float k = (linearSection - 1.0) / (alpha - 1.0);
    params.kA = monitorIntensity * linearSection + monitorIntensity * k;
    params.kB = -monitorIntensity * k * exp(linearSection / k);
    params.kC = -1.0 / (k * monitorIntensity);
    
    return params;
}

FUNC_QUALIFIER float EvaluateGTCurve(float x, GTToneMappingCurveParams params)
{
    if (x < 0.0) return 0.0;
    float weightLinear = smoothstep(0.0, params.midPoint, x);
    float weightToe    = 1.0 - weightLinear;
    float shoulder = params.kA + params.kB * exp(x * params.kC);

    if (x < params.linearSection * params.peakIntensity)
    {
        float toeMapped = params.midPoint * pow(x / params.midPoint, params.toeStrength);
        return weightToe * toeMapped + weightLinear * x;
    }
    else
        return shoulder;
}

FUNC_QUALIFIER float eotfSt2084(float n, float refLum)
{
    if (n < 0.0) n = 0.0;
    if (n > 1.0) n = 1.0;
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float pqC = 10000.0;
    float np = pow(n, 1.0 / m2);
    float l = max(np - c1, 0.0) / (c2 - c3 * np);
    l = pow(l, 1.0 / m1);
    return physicalValueToFrameBufferValue(l * pqC, refLum);
}

FUNC_QUALIFIER float inverseEotfSt2084(float v, float refLum)
{
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float pqC = 10000.0;
    float physical = frameBufferValueToPhysicalValue(v, refLum);
    float y = physical / pqC;
    float ym = pow(y, m1);
    return exp2(m2 * (log2(c1 + c2 * ym) - log2(1.0 + c3 * ym)));
}

FUNC_QUALIFIER void rgbToICtCp(const vec3 rgb, float refLum, out vec3 ictCp)
{
    float l = (rgb.r * 1688.0 + rgb.g * 2146.0 + rgb.b * 262.0) / 4096.0;
    float m = (rgb.r * 683.0 + rgb.g * 2951.0 + rgb.b * 462.0) / 4096.0;
    float s = (rgb.r * 99.0 + rgb.g * 309.0 + rgb.b * 3688.0) / 4096.0;
    float lPQ = inverseEotfSt2084(l, refLum);
    float mPQ = inverseEotfSt2084(m, refLum);
    float sPQ = inverseEotfSt2084(s, refLum);
    ictCp.x = (2048.0 * lPQ + 2048.0 * mPQ) / 4096.0;
    ictCp.y = (6610.0 * lPQ - 13613.0 * mPQ + 7003.0 * sPQ) / 4096.0;
    ictCp.z = (17933.0 * lPQ - 17390.0 * mPQ - 543.0 * sPQ) / 4096.0;
}

FUNC_QUALIFIER void iCtCpToRgb(const vec3 ictCp, float refLum, out vec3 rgb)
{
    float l = ictCp.x + 0.00860904 * ictCp.y + 0.11103 * ictCp.z;
    float m = ictCp.x - 0.00860904 * ictCp.y - 0.11103 * ictCp.z;
    float s = ictCp.x + 0.560031 * ictCp.y - 0.320627 * ictCp.z;
    float lLin = eotfSt2084(l, refLum);
    float mLin = eotfSt2084(m, refLum);
    float sLin = eotfSt2084(s, refLum);
    rgb.r = max(3.43661 * lLin - 2.50645 * mLin + 0.0698454 * sLin, 0.0);
    rgb.g = max(-0.79133 * lLin + 1.9836 * mLin - 0.192271 * sLin, 0.0);
    rgb.b = max(-0.0259499 * lLin - 0.0989137 * mLin + 1.12486 * sLin, 0.0);
}

FUNC_QUALIFIER vec3 GT7TMO(vec3 color)
{
    float paperWhite = ubo.gt7PaperWhite;
    float refLuminance = ubo.gt7ReferenceLuminance;
    if (paperWhite < 1.0) paperWhite = 250.0;
    if (refLuminance < 1.0) refLuminance = 100.0;
    
    float framebufferLuminanceTarget = physicalValueToFrameBufferValue(paperWhite, refLuminance);
    GTToneMappingCurveParams curveParams = InitGTCurve(framebufferLuminanceTarget, 0.25, 0.538, 0.444, 1.280);

    vec3 targetUCS;
    rgbToICtCp(vec3(framebufferLuminanceTarget), refLuminance, targetUCS);
    float framebufferLuminanceTargetUcs = targetUCS.x;

    vec3 ucs;
    rgbToICtCp(color, refLuminance, ucs);

    vec3 skewedRgb = vec3(EvaluateGTCurve(color.r, curveParams),
                          EvaluateGTCurve(color.g, curveParams),
                          EvaluateGTCurve(color.b, curveParams));

    vec3 skewedUcs;
    rgbToICtCp(skewedRgb, refLuminance, skewedUcs);

    float chromaScale = 1.0 - smoothstep(0.98, 1.16, ucs.x / framebufferLuminanceTargetUcs);
    vec3 scaledUcs = vec3(skewedUcs.x, ucs.y * chromaScale, ucs.z * chromaScale);

    vec3 scaledRgb;
    iCtCpToRgb(scaledUcs, refLuminance, scaledRgb);

    float blendRatio = 0.6;
    vec3 outColor = mix(skewedRgb, scaledRgb, blendRatio);
    float correction = 1.0 / physicalValueToFrameBufferValue(paperWhite, refLuminance);
    
    return min(outColor, vec3(framebufferLuminanceTarget)) * correction;
}

#endif // SHADERS_TONEMAPPING_H
