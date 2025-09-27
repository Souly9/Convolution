#extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
struct Material
{
	vec4 baseColor;
	vec4 metallic;
	vec4 emissive;
	vec4 roughness;
	uint diffuseTexture;
	uint normalTexture;
};