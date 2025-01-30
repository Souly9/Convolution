#extension GL_EXT_nonuniform_qualifier : enable
#include "BindingSlots.h"

struct Material
{
	vec4 baseColor;
	vec4 metallic;
	vec4 emissive;
	vec4 roughness;
};

struct PerObjectData
{
	Material material;
};
layout(std140, set = TransformSSBOSet, binding = GlobalTransformDataSSBOSlot) readonly buffer GlobalPerObjectSSBO
{
	mat4 modelMatrices[MAX_ENTITIES];
} globalTransformSSBO;

layout(std430, set = TransformSSBOSet, binding = GlobalObjectDataSSBOSlot) readonly buffer GlobalObjectDataSSBO
{
	Material data[MAX_MATERIALS];
} globalObjectDataSSBO;

layout(std430, set = PassPerObjectDataSet, binding = PassPerObjectDataSSBOSlot) readonly buffer PerPassObjectSSBO
{
	uint transformDataIdx[MAX_ENTITIES];
	uint perObjectDataIdx[MAX_ENTITIES];
} perObjectDataSSBO;