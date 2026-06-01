#ifndef DISNEYPBR_H
#define DISNEYPBR_H

// Disney BRDF model based on: https://github.com/wdas/brdf/blob/main/src/brdfs/disney.brdf


struct SurfaceParameters {
    vec3 baseColor;
    float metallic;
    float subsurface;
    float specular;
    float roughness;
    float specularTint;
    float anisotropic;
    float sheen;
    float sheenTint;
    float clearcoat;
    float clearcoatGloss;
    
    // Extra fields
    float specTrans;
    float ior;
};

SurfaceParameters GetDefaultSurface(vec3 albedo, float roughness, float metallic) {
    SurfaceParameters params;
    params.baseColor = albedo;
    params.metallic = metallic;
    params.subsurface = 0.0;
    params.specular = 0.5;
    params.roughness = clamp(roughness, 0.001, 1.0);
    params.specularTint = 0.0;
    params.anisotropic = 0.0;
    params.sheen = 0.0;
    params.sheenTint = 0.5;
    params.clearcoat = 0.0;
    params.clearcoatGloss = 1.0;
    
    params.specTrans = 0.0;
    params.ior = 1.5;
    return params;
}

float sqr(float x) { return x * x; }

float SchlickFresnel(float u) {
    float m = clamp(1.0 - u, 0.0, 1.0);
    float m2 = m * m;
    return m2 * m2 * m; // pow(m, 5)
}

float GTR1(float NdotH, float a) {
    if (a >= 1.0) return 1.0 / PI;
    float a2 = a * a;
    float t = 1.0 + (a2 - 1.0) * NdotH * NdotH;
    return (a2 - 1.0) / (PI * log(a2) * t);
}

float GTR2(float NdotH, float a) {
    float a2 = a * a;
    float t = 1.0 + (a2 - 1.0) * NdotH * NdotH;
    return a2 / (PI * t * t);
}

float GTR2_aniso(float NdotH, float HdotX, float HdotY, float ax, float ay) {
    return 1.0 / (PI * ax * ay * sqr(sqr(HdotX / ax) + sqr(HdotY / ay) + NdotH * NdotH));
}

float smithG_GGX(float NdotV, float alphaG) {
    float a = alphaG * alphaG;
    float b = NdotV * NdotV;
    return 1.0 / (NdotV + sqrt(a + b - a * b));
}

float smithG_GGX_aniso(float NdotV, float VdotX, float VdotY, float ax, float ay) {
    return 1.0 / (NdotV + sqrt(sqr(VdotX * ax) + sqr(VdotY * ay) + sqr(NdotV)));
}

vec3 EvaluateDisneyPBR(SurfaceParameters surface, vec3 L, vec3 V, vec3 N, vec3 X, vec3 Y) {
    float NdotL = dot(N, L);
    float NdotV = dot(N, V);
    if (NdotL < 0.0 || NdotV < 0.0) return vec3(0.0);

    vec3 H = normalize(L + V);
    float NdotH = dot(N, H);
    float LdotH = dot(L, H);

    vec3 Cdlin = surface.baseColor;
    float Cdlum = 0.3 * Cdlin.r + 0.6 * Cdlin.g + 0.1 * Cdlin.b; // luminance approx.

    vec3 Ctint = Cdlum > 0.0 ? Cdlin / Cdlum : vec3(1.0); // normalize lum. to isolate hue+sat
    vec3 Cspec0 = mix(surface.specular * 0.08 * mix(vec3(1.0), Ctint, surface.specularTint), Cdlin, surface.metallic);
    vec3 Csheen = mix(vec3(1.0), Ctint, surface.sheenTint);

    // Diffuse fresnel - go from 1 at normal incidence to .5 at grazing
    // and mix in diffuse retro-reflection based on roughness
    float FL = SchlickFresnel(NdotL), FV = SchlickFresnel(NdotV);
    float Fd90 = 0.5 + 2.0 * LdotH * LdotH * surface.roughness;
    float Fd = mix(1.0, Fd90, FL) * mix(1.0, Fd90, FV);

    // Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf
    // 1.25 scale is used to (roughly) preserve albedo
    // Fss90 used to "flatten" retroreflection based on roughness
    float Fss90 = LdotH * LdotH * surface.roughness;
    float Fss = mix(1.0, Fss90, FL) * mix(1.0, Fss90, FV);
    float ss = 1.25 * (Fss * (1.0 / (NdotL + NdotV) - 0.5) + 0.5);

    // specular
    float aspect = sqrt(1.0 - surface.anisotropic * 0.9);
    float ax = max(0.001, sqr(surface.roughness) / aspect);
    float ay = max(0.001, sqr(surface.roughness) * aspect);
    float Ds = GTR2_aniso(NdotH, dot(H, X), dot(H, Y), ax, ay);
    float FH = SchlickFresnel(LdotH);
    vec3 Fs = mix(Cspec0, vec3(1.0), FH);
    float Gs;
    Gs = smithG_GGX_aniso(NdotL, dot(L, X), dot(L, Y), ax, ay);
    Gs *= smithG_GGX_aniso(NdotV, dot(V, X), dot(V, Y), ax, ay);

    // sheen
    vec3 Fsheen = FH * surface.sheen * Csheen;

    // clearcoat (ior = 1.5 -> F0 = 0.04)
    float Dr = GTR1(NdotH, mix(0.1, 0.001, surface.clearcoatGloss));
    float Fr = mix(0.04, 1.0, FH);
    float Gr = smithG_GGX(NdotL, 0.25) * smithG_GGX(NdotV, 0.25);

    vec3 diffuse = InvPi * mix(Fd, ss, surface.subsurface) * Cdlin;
    vec3 sheen = Fsheen;
    vec3 specular = Gs * Fs * Ds;
    vec3 clearcoat = vec3(0.25 * surface.clearcoat * Gr * Fr * Dr);

    return (diffuse + sheen) * (1.0 - surface.metallic) + specular + clearcoat;
}

float compAttentuation(vec3 worldPos, vec3 lightPos, float lightRange) {
    if (lightRange <= MIN_DENOMINATOR) return 0.0;
    float dist = length(lightPos - worldPos);
    float attenuation = 1.0 / max(dist * dist, MIN_DENOMINATOR);
    float factor = clamp(1.0 - pow(dist / lightRange, 4.0), 0.0, 1.0);
    factor *= factor;
    return attenuation * factor;
}

vec3 computePBRDirectLight(vec3 L, vec3 radiance, vec3 viewDir, vec3 normal, SurfaceParameters surface) {
    vec3 wi = L;
    vec3 wo = viewDir;
    float dotNL = max(dot(wi, normal), 0.0);
    if (dotNL <= 0.0) return vec3(0.0);

    // Default tangent/bitangent (since G-buffer lacks these)
    vec3 up = abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, normal));
    vec3 bitangent = cross(normal, tangent);

    vec3 reflectance = EvaluateDisneyPBR(surface, wi, wo, normal, tangent, bitangent);
    return reflectance * dotNL * radiance;
}

vec3 computePointLight(vec3 worldPos, vec3 lightPos, float lightRange, vec3 viewDir, vec3 normal, vec3 lightColor, SurfaceParameters surface) {
    vec3 L = normalize(lightPos - worldPos);
    float attenuation = compAttentuation(worldPos, lightPos, lightRange);
    return computePBRDirectLight(L, lightColor * attenuation, viewDir, normal, surface);
}

vec3 computeDirLight(vec3 lightDir, vec3 viewDir, vec3 normal, vec3 lightColor, SurfaceParameters surface) {
    return computePBRDirectLight(normalize(-lightDir), lightColor, viewDir, normal, surface);
}

vec3 computeAmbient(vec3 color, float intensity) {
    return intensity * color;
}

#endif // DISNEYPBR_H
 
