#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#include "BindingSlots.h"
#include "Material.h"
#include "IndirectDrawStructs.h"

layout(std140, set = TransformSSBOSet, binding = GlobalTransformDataSSBOSlot) readonly buffer GlobalPerObjectSSBO
{
	mat4 modelMatrices[MAX_ENTITIES];
} globalTransformSSBO;

layout(scalar, set = TransformSSBOSet, binding = GlobalObjectDataSSBOSlot) readonly buffer GlobalObjectDataSSBO
{
	Material materials[MAX_MATERIALS];
} globalObjectDataSSBO;

layout(scalar, set = TransformSSBOSet, binding = GlobalInstanceDataSSBOSlot) readonly buffer GlobalInstanceDataSSBO
{
	InstanceData instances[MAX_ENTITIES];
} globalInstanceDataSSBO;
