#include "../Utils/Math.h"

// GT7 functions based on the reference implementation:
// https://blog.selfshadow.com/publications/s2025-shading-course/pdi/supplemental/gt7_tone_mapping.cpp

#define GRAN_TURISMO_SDR_PAPER_WHITE 250.0 // cd/m^2
#define REFERENCE_LUMINANCE 100.0 // cd/m^2 <-> 1.0f
#define JZAZBZ_EXPONENT_SCALE_FACTOR 1.7 // Scale factor for exponent

float frameBufferValueToPhysicalValue(float fbValue, float refLum)
{
    // Converts linear frame-buffer value to physical luminance (cd/m^2)
    // where 1.0 corresponds to REFERENCE_LUMINANCE (e.g., 100 cd/m^2).
    return fbValue * refLum;
}

float physicalValueToFrameBufferValue(float physical, float refLum)
{
    // Converts physical luminance (cd/m^2) to a linear frame-buffer value,
    // where 1.0 corresponds to REFERENCE_LUMINANCE (e.g., 100 cd/m^2).
    return physical / refLum;
}

// -----------------------------------------------------------------------------
// "GT Tone Mapping" curve with convergent shoulder.
// -----------------------------------------------------------------------------
// Stateless version of curve evaluation
// Params are: P, a, m, l, c, b (standard parameter names) -> mapped from struct
// GTToneMappingCurveV2 members:
// peakIntensity_ -> monitorIntensity
// alpha_         -> alpha
// midPoint_      -> grayPoint
// linearSection_ -> linearSection
// toeStrength_   -> toeStrength
// 
// k, kA, kB, kC are just computed every frame
// TODO: Optimize?
struct GTToneMappingCurveParams
{
    float peakIntensity;
    float alpha;
    float midPoint;
    float linearSection;
    float toeStrength;
    
    // Derived constants
    float kA, kB, kC;
};

GTToneMappingCurveParams InitGTCurve(float monitorIntensity, float alpha, float midPoint, float linearSection, float toeStrength)
{
    GTToneMappingCurveParams params;
    params.peakIntensity = monitorIntensity;
    params.alpha = alpha;
    params.midPoint = midPoint;
    params.linearSection = linearSection;
    params.toeStrength = toeStrength;

    // Pre-compute constants for the shoulder region.
    float k = (linearSection - 1.0) / (alpha - 1.0);
    params.kA = monitorIntensity * linearSection + params.peakIntensity * k;
    params.kB = -monitorIntensity * k * exp(linearSection / k);
    params.kC = -1.0 / (k * params.peakIntensity);
    
    return params;
}

float EvaluateGTCurve(float x, GTToneMappingCurveParams params)
{
    if (x < 0.0)
    {
        return 0.0;
    }

    float weightLinear = smoothStep(x, 0.0, params.midPoint);
    float weightToe    = 1.0 - weightLinear;

    // Shoulder mapping for highlights.
    float shoulder = params.kA + params.kB * exp(x * params.kC);

    if (x < params.linearSection * params.peakIntensity)
    {
        float toeMapped = params.midPoint * pow(x / params.midPoint, params.toeStrength);
        return weightToe * toeMapped + weightLinear * x;
    }
    else
    {
        return shoulder;
    }
}

float chromaCurve(float x, float a, float b)
{
    return 1.0 - smoothStep(x, a, b);
}

// -----------------------------------------------------------------------------
// EOTF / inverse-EOTF for ST-2084 (PQ).
// Note: Introduce exponentScaleFactor to allow scaling of the exponent in the EOTF for Jzazbz.
// -----------------------------------------------------------------------------
float eotfSt2084(float n, float exponentScaleFactor, float refLum)
{
    if (n < 0.0) n = 0.0;
    if (n > 1.0) n = 1.0;

    const float m1  = 0.1593017578125;                // (2610 / 4096) / 4
    const float m2  = 78.84375 * exponentScaleFactor; // (2523 / 4096) * 128
    const float c1  = 0.8359375;                      // 3424 / 4096
    const float c2  = 18.8515625;                     // (2413 / 4096) * 32
    const float c3  = 18.6875;                        // (2392 / 4096) * 32
    const float pqC = 10000.0;                        // Maximum luminance supported by PQ (cd/m^2)

    float np = pow(n, 1.0 / m2);
    float l  = np - c1;

    if (l < 0.0) l = 0.0;

    l = l / (c2 - c3 * np);
    l = pow(l, 1.0 / m1);

    return physicalValueToFrameBufferValue(l * pqC, refLum);
}

float inverseEotfSt2084(float v, float exponentScaleFactor, float refLum)
{
    const float m1  = 0.1593017578125;
    const float m2  = 78.84375 * exponentScaleFactor;
    const float c1  = 0.8359375;
    const float c2  = 18.8515625;
    const float c3  = 18.6875;
    const float pqC = 10000.0;

    // Convert the frame-buffer linear scale into absolute luminance (cd/m^2).
    float physical = frameBufferValueToPhysicalValue(v, refLum);
    float y        = physical / pqC; // Normalize for the ST-2084 curve

    float ym = pow(y, m1);
    return exp2(m2 * (log2(c1 + c2 * ym) - log2(1.0 + c3 * ym)));
}

// -----------------------------------------------------------------------------
// ICtCp conversion.
// Reference: ITU-T T.302 (https://www.itu.int/rec/T-REC-T.302/en)
// -----------------------------------------------------------------------------
void rgbToICtCp(const vec3 rgb, float refLum, out vec3 ictCp) // Input: linear Rec.2020
{
    float l = (rgb.r * 1688.0 + rgb.g * 2146.0 + rgb.b * 262.0) / 4096.0;
    float m = (rgb.r * 683.0 + rgb.g * 2951.0 + rgb.b * 462.0) / 4096.0;
    float s = (rgb.r * 99.0 + rgb.g * 309.0 + rgb.b * 3688.0) / 4096.0;

    float lPQ = inverseEotfSt2084(l, 1.0, refLum);
    float mPQ = inverseEotfSt2084(m, 1.0, refLum);
    float sPQ = inverseEotfSt2084(s, 1.0, refLum);

    ictCp.x = (2048.0 * lPQ + 2048.0 * mPQ) / 4096.0;
    ictCp.y = (6610.0 * lPQ - 13613.0 * mPQ + 7003.0 * sPQ) / 4096.0;
    ictCp.z = (17933.0 * lPQ - 17390.0 * mPQ - 543.0 * sPQ) / 4096.0;
}

void iCtCpToRgb(const vec3 ictCp, float refLum, out vec3 rgb) // Output: linear Rec.2020
{
    float l = ictCp.x + 0.00860904 * ictCp.y + 0.11103 * ictCp.z;
    float m = ictCp.x - 0.00860904 * ictCp.y - 0.11103 * ictCp.z;
    float s = ictCp.x + 0.560031 * ictCp.y - 0.320627 * ictCp.z;

    float lLin = eotfSt2084(l, 1.0, refLum);
    float mLin = eotfSt2084(m, 1.0, refLum);
    float sLin = eotfSt2084(s, 1.0, refLum);

    rgb.r = max(3.43661 * lLin - 2.50645 * mLin + 0.0698454 * sLin, 0.0);
    rgb.g = max(-0.79133 * lLin + 1.9836 * mLin - 0.192271 * sLin, 0.0);
    rgb.b = max(-0.0259499 * lLin - 0.0989137 * mLin + 1.12486 * sLin, 0.0);
}

// -----------------------------------------------------------------------------
// Unified color space (UCS): ICtCp
// -----------------------------------------------------------------------------
void rgbToUcs(const vec3 rgb, float refLum, out vec3 ucs)
{
    rgbToICtCp(rgb, refLum, ucs);
}

void ucsToRgb(const vec3 ucs, float refLum, out vec3 rgb)
{
    iCtCpToRgb(ucs, refLum, rgb);
}
