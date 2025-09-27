// Implemented based off the presentation and https://www.shadertoy.com/view/lslGzl
vec3 Uncharted2Tonemap(vec3 color, float exposure)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	color *= exposure;
	color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
	float white = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
	color /= white;
	return color;
}

vec3 GammaCorrect(vec3 color)
{
	return pow(color, vec3(1.0 / 2.2));
}