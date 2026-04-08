#ifndef SHADERS_GEOMETRY_PASS_DATA_H
#define SHADERS_GEOMETRY_PASS_DATA_H
#extension GL_EXT_scalar_block_layout : enable
#include "Scene.h"
#include "Material.h"


layout(std140, set = TransformSSBOSet, binding = GlobalTransformDataSSBOSlot) readonly buffer GlobalPerObjectSSBO
{
    mat4 modelMatrices[MAX_ENTITIES];
}
globalTransformSSBO;

layout(scalar, set = TransformSSBOSet, binding = GlobalObjectDataSSBOSlot) readonly buffer GlobalObjectDataSSBO
{
    Material materials[MAX_MATERIALS];
}
globalObjectDataSSBO;

layout(scalar, set = TransformSSBOSet, binding = GlobalInstanceDataSSBOSlot) readonly buffer GlobalInstanceDataSSBO
{
    InstanceData instances[MAX_ENTITIES];
}
globalInstanceDataSSBO;

layout(std140, set = TransformSSBOSet, binding = PrevGlobalTransformDataSSBOSlot) readonly buffer PrevGlobalPerObjectSSBO
{
    mat4 prevModelMatrices[MAX_ENTITIES];
}
prevGlobalTransformSSBO;

layout(scalar, set = PassPerObjectDataSet, binding = PassPerObjectDataSSBOSlot) readonly buffer PerPassObjectSSBO
{
    uint transformDataIdx[MAX_ENTITIES];
}
perObjectDataSSBO;

FUNC_QUALIFIER mat4 FetchWorldMatrix(uint glInstanceIdx)
{
    uint instanceIdx = perObjectDataSSBO.transformDataIdx[glInstanceIdx];
    InstanceData iData = globalInstanceDataSSBO.instances[instanceIdx];
    uint transformIdx = GetTransformIdx(iData);
    mat4 worldMat = globalTransformSSBO.modelMatrices[transformIdx];
    return worldMat;
}

FUNC_QUALIFIER bool IsInstanceVisible(uint glInstanceIdx)
{
    uint instanceIdx = perObjectDataSSBO.transformDataIdx[glInstanceIdx];
    InstanceData iData = globalInstanceDataSSBO.instances[instanceIdx];
    return IsVisible(iData);
}
#endif // SHADERS_GEOMETRY_PASS_DATA_H
