#ifndef UNREALPBR_H
#define UNREALPBR_H

// Unreal Engine PBR BRDF model
// Based on: https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes.pdf

const float PI = 3.14159265358979323846;
const float InvPi = 1.0 / PI;

struct SurfaceParameters {
    vec3 baseColor;
    float metallic;
    float roughness;
    float specular;
    float subsurface;
    float clearcoat;
    float clearcoatGloss;
    float sheen;
    float anisotropic;
};

SurfaceParameters GetDefaultSurface(vec3 albedo, float roughness, float metallic) {
    SurfaceParameters params;
    params.baseColor = albedo;
    params.metallic = metallic;
    params.roughness = clamp(roughness, 0.045, 1.0);
    params.specular = 0.5;
    params.subsurface = 0.0;
    params.clearcoat = 0.0;
    params.clearcoatGloss = 1.0;
    params.sheen = 0.0;
    params.anisotropic = 0.0;
    return params;
}

// GGX / Trowbridge-Reitz
float D_GGX(float NdotH, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

// Schlick-GGX
float G1_SchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

// Smith's method using Schlick-GGX
float G_Smith(float NdotV, float NdotL, float roughness) {
    return G1_SchlickGGX(NdotV, roughness) * G1_SchlickGGX(NdotL, roughness);
}

// Schlick's approximation
vec3 F_Schlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 EvaluateUnrealPBR(SurfaceParameters surface, vec3 L, vec3 V, vec3 N) {
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    if (NdotL <= 0.0) return vec3(0.0);

    // F0 for non-metals is 0.04 (standard) scaled by specular parameter (default 0.5 gives 0.04)
    vec3 F0 = mix(vec3(0.08 * surface.specular), surface.baseColor, surface.metallic);

    float D = D_GGX(NdotH, surface.roughness);
    float G = G_Smith(NdotV, NdotL, surface.roughness);
    vec3 F = F_Schlick(VdotH, F0);

    vec3 specNumerator = D * G * F;
    float specDenominator = 4.0 * NdotV * NdotL + 0.001; // Avoid division by zero
    vec3 specular = specNumerator / specDenominator;

    // kS is Fresnel term
    vec3 kS = F;
    // kD is remaining energy
    vec3 kD = (vec3(1.0) - kS) * (1.0 - surface.metallic);

    vec3 diffuse = kD * surface.baseColor * InvPi;

    return diffuse + specular;
}

// Helper for attenuation matching DisneyPBR.h (energy preserving)
float compAttentuation(vec3 worldPos, vec3 lightPos, float lightRange) {
    float dist = length(lightPos - worldPos);
    float attenuation = 1.0 / max(dist * dist, 0.0001);
    float factor = clamp(1.0 - pow(dist / lightRange, 4.0), 0.0, 1.0);
    factor *= factor;
    return attenuation * factor;
}

vec3 computePBRDirectLight(vec3 L, vec3 radiance, vec3 viewDir, vec3 normal, SurfaceParameters surface) {
    float dotNL = max(dot(L, normal), 0.0);
    if (dotNL <= 0.0) return vec3(0.0);

    vec3 reflectance = EvaluateUnrealPBR(surface, L, viewDir, normal);
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

#endif // UNREALPBR_H
