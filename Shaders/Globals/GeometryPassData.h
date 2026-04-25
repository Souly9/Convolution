#ifndef SHADERS_GEOMETRY_PASS_DATA_H
#define SHADERS_GEOMETRY_PASS_DATA_H
#ifndef __cplusplus
#extension GL_EXT_scalar_block_layout : enable
#endif
#include "Scene.h"
#include "Material.h"

STRUCTDECL(GlobalTransformBuffer)
    STRUCTFIELD_ARRAY(mat4, modelMatrices, MAX_ENTITIES)
STRUCTEND()

STRUCTDECL(GlobalObjectDataBuffer)
    STRUCTFIELD_ARRAY(Material, materials, MAX_MATERIALS)
STRUCTEND()

STRUCTDECL(GlobalInstanceDataBuffer)
    STRUCTFIELD_ARRAY(InstanceData, instances, MAX_ENTITIES)
STRUCTEND()

STRUCTDECL(PrevGlobalTransformBuffer)
    STRUCTFIELD_ARRAY(mat4, prevModelMatrices, MAX_ENTITIES)
STRUCTEND()

STRUCTDECL(PerPassObjectBuffer)
    STRUCTFIELD_ARRAY(uint, transformDataIdx, MAX_ENTITIES)
STRUCTEND()

#ifndef __cplusplus
layout(std140, set = TransformSSBOSet, binding = GlobalTransformDataSSBOSlot) readonly buffer GlobalPerObjectSSBO
{
    GlobalTransformBuffer globalTransformSSBO;
};

layout(scalar, set = TransformSSBOSet, binding = GlobalObjectDataSSBOSlot) readonly buffer GlobalObjectDataSSBO
{
    GlobalObjectDataBuffer globalObjectDataSSBO;
};

layout(scalar, set = TransformSSBOSet, binding = GlobalInstanceDataSSBOSlot) readonly buffer GlobalInstanceDataSSBO
{
    GlobalInstanceDataBuffer globalInstanceDataSSBO;
};

layout(std140, set = TransformSSBOSet, binding = PrevGlobalTransformDataSSBOSlot) readonly buffer PrevGlobalPerObjectSSBO
{
    PrevGlobalTransformBuffer prevGlobalTransformSSBO;
};

layout(scalar, set = PassPerObjectDataSet, binding = PassPerObjectDataSSBOSlot) readonly buffer PerPassObjectSSBO
{
    PerPassObjectBuffer perObjectDataSSBO;
};
#endif

#ifndef __cplusplus
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
#endif
#endif // SHADERS_GEOMETRY_PASS_DATA_H
