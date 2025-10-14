#pragma once

struct MaterialProperties
{
	mathstl::Vector4 baseColor{ 1.0 };
	mathstl::Vector4 metallic { 0.05 };
	mathstl::Vector4 emissive;
	mathstl::Vector4 roughness { 0.4 };
};
struct Material
{
	MaterialProperties properties;
	u32 diffuseTexture;
	u32 normalTexture;

	u32 p;
	u32 p2;
};
