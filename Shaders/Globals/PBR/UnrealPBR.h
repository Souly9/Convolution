const float PI = 3.14159265359;
// Hardcoded radius for pointlights for simplification
const float PointLightRadius = 500.0;
const float MIN_DENOMINATOR = 0.000001;

// Epic's attentuation function 
float compAttentuation(vec3 worldPos, vec3 lightPos)
{
	float dist = length(lightPos - worldPos);

	// 1. Inverse Square Falloff
	float attenuation = 1.0 / max(dist * dist, MIN_DENOMINATOR);

	// 2. Smooth Step Radius Cutoff (Epic's $1 - (d/R)^4)^2$ approximation)
	float factor = clamp(1.0 - pow(dist / PointLightRadius, 4.0), 0.0, 1.0);
	factor *= factor; 

	return attenuation * factor;
}

/*
* Implementation of Epic's UE4 BRDF
*/

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	// Clamp cosTheta to prevent artifacts when it approaches 0 or is negative
	cosTheta = clamp(cosTheta, 0.0, 1.0);
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float DistributionGGX(vec3 normal, vec3 halfway, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(normal, halfway), 0.0);
	float NdotH2 = NdotH * NdotH;

	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;

	return a2 / denom;
}

float GeometrySchlickGGX(float NdotV, float k)
{
	float denom = NdotV * (1.0 - k) + k;

	return NdotV / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);

	float k = roughness * 0.5;

	float schlickV = GeometrySchlickGGX(NdotV, k);
	float schlickL = GeometrySchlickGGX(NdotL, k);
	return (schlickV * schlickL);
}

vec3 computePBRDirectLight(vec3 L, vec3 radiance, vec3 viewDir, vec3 normal,
	vec3 diffuseAlbedo, float roughness, float metallic)
{
	vec3 H = normalize(viewDir + L); // Halfway vector calculation
	float NdotL = max(dot(normal, L), 0.0);
	float HdotV = max(dot(H, viewDir), 0.0);

	// F0 (Base Reflection)
	vec3 F0 = vec3(0.04);
	F0 = mix(F0, diffuseAlbedo, metallic);

	// 1. Fresnel (F)
	vec3 F = fresnelSchlick(HdotV, F0);

	// 2. Normal Distribution Function (D)
	float NDF = DistributionGGX(normal, H, roughness);

	// 3. Geometry (G)
	float G = GeometrySmith(normal, viewDir, L, roughness);

	// Specular BRDF (D * G * F) / (4 * NdotV * NdotL)
	vec3 numerator = NDF * G * F;
	float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * NdotL;
	vec3 specular = numerator / max(denominator, MIN_DENOMINATOR);

	// Diffuse component (Energy Conservation)
	// kD = (1 - F_specular) * (1 - metallic)
	vec3 kD = vec3(1.0) - F;
	kD *= 1.0 - metallic;

	// Final PBR equation: (kD * DiffuseBRDF + SpecularBRDF) * Radiance * NdotL
	vec3 diffuseBRDF = diffuseAlbedo / PI; // Lambertian diffuse
	return (kD * diffuseBRDF + specular) * radiance * NdotL;
}


// --- Light Type Wrappers ---
vec3 computePointLight(vec3 worldPos, vec3 lightPos, vec3 viewDir, vec3 normal,
	vec3 lightColor, vec3 diffuseAlbedo, float roughness, float metallic)
{
	vec3 L = normalize(lightPos - worldPos);
	float attenuation = compAttentuation(worldPos, lightPos);
	vec3 radiance = lightColor * attenuation;

	return computePBRDirectLight(L, radiance, viewDir, normal,
		diffuseAlbedo, roughness, metallic);
}

vec3 computeDirLight(vec3 lightDir, vec3 viewDir, vec3 normal,
	vec3 lightColor, vec3 diffuseAlbedo, float roughness, float metallic)
{
	// (attenuation is 1.0)
	vec3 L = normalize(-lightDir);
	vec3 radiance = lightColor;

	return computePBRDirectLight(L, radiance, viewDir, normal,
		diffuseAlbedo, roughness, metallic);
}

vec3 computeAmbient(vec3 color)
{
	return 0.1 * color;
}

// Implemented with https://www.shadertoy.com/view/lslGzl
vec3 ModifiedReinhardTMO(vec3 color)
{
	float exposure = 1.;
	color *= exposure;
	float white = 2.4;
	float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
	float toneMappedLuma = luma * (1.0 + luma / (white * white)) / (1.0 + luma);
	color *= toneMappedLuma / luma;
	return color;
}

vec3 AcesTMO(vec3 color)
{
	float exposure = 1.;
	color *= exposure;
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

// Implemented based off the presentation and https://www.shadertoy.com/view/lslGzl
vec3 Uncharted2TMO(vec3 color)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	float exposure = 1.;
	color *= exposure;
	color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
	float white = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
	color /= white;
	return color;
}