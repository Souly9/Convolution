// Disney BRDF models based on this post: https://schuttejoe.github.io/post/disneybsdf/

const float PI = 3.14159265359;
const float InvPi = 1.0 / PI;
const float MIN_DENOMINATOR = 0.000001;

struct SurfaceParameters {
    vec3 baseColor;
    float roughness;
    float metallic;
    
    float anisotropic;
    float specularTint;
    float ior;
    float clearcoat;
    float clearcoatGloss;
    float sheen;
    float sheenTint;
    float specTrans;
    float flatness;
};

SurfaceParameters GetDefaultSurface(vec3 albedo, float roughness, float metallic) {
    SurfaceParameters params;
    params.baseColor = albedo;
    params.roughness = clamp(roughness, 0.001, 1.0);
    params.metallic = metallic;
    
    params.anisotropic = 0.0;
    params.specularTint = 0.0;
    params.ior = 1.5;
    params.clearcoat = 0.0;
    params.clearcoatGloss = 1.0;
    params.sheen = 0.0;
    params.sheenTint = 0.5;
    params.specTrans = 0.0;
    params.flatness = 0.0;
    return params;
}

float sqr(float x) { return x * x; }

float SchlickWeight(float u) {
    float m = clamp(1.0 - u, 0.0, 1.0);
    float m2 = m * m;
    return m2 * m2 * m;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * SchlickWeight(cosTheta);
}

float GgxAnisotropicD(vec3 wm, vec3 tangent, vec3 bitangent, vec3 normal, float ax, float ay) {
    float dotHX = dot(wm, tangent);
    float dotHY = dot(wm, bitangent);
    float dotHN = dot(wm, normal);
    float dotHX2 = sqr(dotHX);
    float dotHY2 = sqr(dotHY);
    float cos2Theta = sqr(dotHN);
    
    return 1.0 / (PI * ax * ay * sqr(dotHX2 / sqr(ax) + dotHY2 / sqr(ay) + cos2Theta));
}

float SeparableSmithGGXG1(vec3 w, vec3 wm, vec3 tangent, vec3 bitangent, vec3 normal, float ax, float ay) {
    float dotHW = dot(w, wm);
    if (dotHW <= 0.0) return 0.0;
    
    float dotWX = dot(w, tangent);
    float dotWY = dot(w, bitangent);
    float dotWN = dot(w, normal);
    
    float a2Tan2Theta = (sqr(dotWX * ax) + sqr(dotWY * ay)) / max(sqr(dotWN), MIN_DENOMINATOR);
    float lambda = 0.5 * (-1.0 + sqrt(1.0 + a2Tan2Theta));
    return 1.0 / (1.0 + lambda);
}

vec3 CalculateTint(vec3 baseColor) {
    float luminance = dot(baseColor, vec3(0.2126, 0.7152, 0.0722));
    return luminance > 0.0 ? baseColor / luminance : vec3(1.0);
}

float SchlickR0FromRelativeIOR(float ior) {
    return sqr((ior - 1.0) / (ior + 1.0));
}

float DielectricFresnel(float cosThetaI, float eta) {
    float sinThetaI2 = 1.0 - cosThetaI * cosThetaI;
    float sinThetaT2 = sinThetaI2 / (eta * eta);
    if (sinThetaT2 >= 1.0) return 1.0;
    float cosThetaT = sqrt(max(0.0, 1.0 - sinThetaT2));
    float Rs = (cosThetaI - eta * cosThetaT) / (cosThetaI + eta * cosThetaT);
    float Rp = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);
    return 0.5 * (Rs * Rs + Rp * Rp);
}

vec3 DisneyFresnel(SurfaceParameters surface, vec3 wo, vec3 wm, vec3 wi) {
    float dotHV = abs(dot(wm, wo));
    vec3 tint = CalculateTint(surface.baseColor);
    vec3 R0 = vec3(SchlickR0FromRelativeIOR(surface.ior)) * mix(vec3(1.0), tint, surface.specularTint);
    R0 = mix(R0, surface.baseColor, surface.metallic);
    
    float dielectricFresnel = DielectricFresnel(dotHV, surface.ior);
    vec3 metallicFresnel = fresnelSchlick(dotHV, R0);
    return mix(vec3(dielectricFresnel), metallicFresnel, surface.metallic);
}

vec3 EvaluateDisneyBRDF(SurfaceParameters surface, vec3 wo, vec3 wm, vec3 wi, vec3 normal, vec3 tangent, vec3 bitangent) {
    float dotNL = dot(wi, normal);
    float dotNV = dot(wo, normal);
    if (dotNL <= 0.0 || dotNV <= 0.0) return vec3(0.0);
    
    float aspect = sqrt(1.0 - 0.9 * surface.anisotropic);
    float ax = max(0.001, sqr(surface.roughness) / aspect);
    float ay = max(0.001, sqr(surface.roughness) * aspect);
    
    float d = GgxAnisotropicD(wm, tangent, bitangent, normal, ax, ay);
    float gl = SeparableSmithGGXG1(wi, wm, tangent, bitangent, normal, ax, ay);
    float gv = SeparableSmithGGXG1(wo, wm, tangent, bitangent, normal, ax, ay);
    vec3 f = DisneyFresnel(surface, wo, wm, wi);
    
    return (d * gl * gv * f) / max(4.0 * dotNL * dotNV, MIN_DENOMINATOR);
}

vec3 EvaluateDisneyDiffuse(SurfaceParameters surface, vec3 wo, vec3 wm, vec3 wi, vec3 normal) {
    float dotNL = max(dot(wi, normal), 0.0);
    float dotNV = max(dot(wo, normal), 0.0);
    float fl = SchlickWeight(dotNL);
    float fv = SchlickWeight(dotNV);
    
    float hanrahanKrueger = 0.0;
    if (surface.flatness > 0.0) {
        float roughness2 = sqr(surface.roughness);
        float dotHL = dot(wm, wi);
        float fss90 = dotHL * dotHL * roughness2;
        float fss = mix(1.0, fss90, fl) * mix(1.0, fss90, fv);
        hanrahanKrueger = 1.25 * (fss * (1.0 / max(dotNL + dotNV, MIN_DENOMINATOR) - 0.5) + 0.5);
    }
    
    float dotVH = max(dot(wo, wm), 0.0);
    float rr = 2.0 * surface.roughness * dotVH * dotVH;
    float retro = rr * (fl + fv + fl * fv * (rr - 1.0));
    
    float subsurfaceApprox = mix(1.0, hanrahanKrueger, surface.flatness);
    float diffuse = (retro + subsurfaceApprox * (1.0 - 0.5 * fl) * (1.0 - 0.5 * fv));
    return surface.baseColor * InvPi * diffuse;
}

float compAttentuation(vec3 worldPos, vec3 lightPos, float lightRange)
{
    if (lightRange <= MIN_DENOMINATOR)
        return 0.0;

    float dist = length(lightPos - worldPos);

    float attenuation = 1.0 / max(dist * dist, MIN_DENOMINATOR);
    float factor = clamp(1.0 - pow(dist / lightRange, 4.0), 0.0, 1.0);
    factor *= factor;

    return attenuation * factor;
}

vec3 computePBRDirectLight(
    vec3 L, vec3 radiance, vec3 viewDir, vec3 normal, SurfaceParameters surface)
{
    vec3 wi = L;
    vec3 wo = viewDir;
    
    vec3 wm = normalize(wo + wi);
    if(length(wo + wi) < MIN_DENOMINATOR) {
        wm = normal;
    }
    
    float dotNL = max(dot(wi, normal), 0.0);
    float dotNV = max(dot(wo, normal), 0.0);
    if(dotNL <= 0.0 || dotNV <= 0.0) return vec3(0.0);

    // Default tangent/bitangent (since G-buffer lacks these)
    vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = cross(normal, tangent);
    
    vec3 specular = EvaluateDisneyBRDF(surface, wo, wm, wi, normal, tangent, bitangent);
    vec3 diffuse = EvaluateDisneyDiffuse(surface, wo, wm, wi, normal);
    
    float diffuseWeight = (1.0 - surface.metallic) * (1.0 - surface.specTrans);
    vec3 reflectance = diffuseWeight * diffuse + specular;
    
    return reflectance * dotNL * radiance;
}

vec3 computePointLight(vec3 worldPos,
                       vec3 lightPos,
                       float lightRange,
                       vec3 viewDir,
                       vec3 normal,
                       vec3 lightColor,
                       SurfaceParameters surface)
{
    vec3 L = normalize(lightPos - worldPos);
    float attenuation = compAttentuation(worldPos, lightPos, lightRange);
    vec3 radiance = lightColor * attenuation;

    return computePBRDirectLight(L, radiance, viewDir, normal, surface);
}

vec3 computeDirLight(
    vec3 lightDir, vec3 viewDir, vec3 normal, vec3 lightColor, SurfaceParameters surface)
{
    vec3 L = normalize(-lightDir);
    vec3 radiance = lightColor;

    return computePBRDirectLight(L, radiance, viewDir, normal, surface);
}

vec3 computeAmbient(vec3 color, float intensity)
{
    return intensity * color;
}

// Tone Mapping functions normally in UnrealPBR.h needed by Simple.frag
vec3 ModifiedReinhardTMO(vec3 color)
{
    float white = 2.4;
    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    float toneMappedLuma = luma * (1.0 + luma / (white * white)) / (1.0 + luma);
    color *= toneMappedLuma / max(luma, MIN_DENOMINATOR);
    return color;
}

vec3 AcesTMO(vec3 color)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

vec3 Uncharted2TMO(vec3 color)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    float W = 11.2;
    color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
    float white = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
    color /= white;
    return color;
}
