#ifndef SHADERS_SCENE_H
#define SHADERS_SCENE_H
#ifndef __cplusplus
#extension GL_EXT_scalar_block_layout : enable
#endif

#include "Common.h"
#include "Material.h"

#ifndef __cplusplus
struct IndexedIndirectDrawCmd
{
    uint    indexCount;
    uint    instanceCount;
    uint    firstIndex;
    int    vertexOffset;
    uint    firstInstance;
};
#endif

STRUCTDECL(MeshResourceData)
    STRUCTFIELD(uint, vertBufferOffset)
    STRUCTFIELD(uint, indexBufferOffset)
    STRUCTFIELD(uint, vertCount)
    STRUCTFIELD(uint, indexCount)
STRUCTEND()

STRUCTDECL(InstanceData)
    STRUCTFIELD(MeshResourceData, drawData)
    STRUCTFIELD(vec4, aabbCenterTransIdx)
    STRUCTFIELD(vec4, aabbExtentsMatIdx)
    STRUCTFIELD(uint, flags)

#ifdef __cplusplus
    void SetTransformIdx(u32 idx)
    {
        aabbCenterTransIdx.w = static_cast<f32>(idx);
    }

    void SetMaterialIdx(u32 idx)
    {
        aabbExtentsMatIdx.w = static_cast<f32>(idx);
    }

    void SetVisible(bool visible)
    {
        flags = visible ? 1u : 0u;
    }
#endif
STRUCTEND()

FUNC_QUALIFIER uint GetTransformIdx(InstanceData data)
{
    return uint(data.aabbCenterTransIdx.w);
}

FUNC_QUALIFIER uint GetMaterialIdx(InstanceData data)
{
    return uint(data.aabbExtentsMatIdx.w);
}

FUNC_QUALIFIER bool IsVisible(InstanceData data)
{
    return (data.flags & 1) != 0;
}

STRUCTDECL(Light)
    STRUCTFIELD(vec4, position)
    STRUCTFIELD(vec4, direction)
    STRUCTFIELD(vec4, color)
    STRUCTFIELD(vec4, cutoff)
STRUCTEND()

STRUCTDECL(DirectionalLight)
    STRUCTFIELD(vec4, direction)
    STRUCTFIELD(vec4, color)
STRUCTEND()

STRUCTDECL(SharedDataUBO)
    STRUCTFIELD(mat4, view)
    STRUCTFIELD(mat4, projection)
    STRUCTFIELD(mat4, viewProjection)
    STRUCTFIELD(mat4, viewProjectionInverse)
    STRUCTFIELD(mat4, viewInverse)
    STRUCTFIELD(mat4, projectionInverse)
    STRUCTFIELD(mat4, jitteredProjection)
    STRUCTFIELD(mat4, jitteredViewProjectionInverse)
    STRUCTFIELD(mat4, prevView)
    STRUCTFIELD(mat4, prevProjection)
    STRUCTFIELD(mat4, prevViewProjection)
    STRUCTFIELD(mat4, prevJitteredProjection)
    STRUCTFIELD(vec4, viewPos)
    STRUCTFIELD(vec2, renderResolution)
    STRUCTFIELD(vec2, jitterOffset)

    // CSM shadow parameters
    STRUCTFIELD_ARRAY(mat4, csmViewMatrices, 16)
    STRUCTFIELD_ARRAY(vec4, cascadeSplits, 4)
    STRUCTFIELD(int, cascadeCount)
    STRUCTFIELD(BindlessTextureHandle, screenSpaceShadows)

    // Debug & Global settings
    STRUCTFIELD(uint, debugFlags)
    STRUCTFIELD(int, debugViewMode)
    STRUCTFIELD(float, exposure)
    STRUCTFIELD(int, toneMapperType)
    STRUCTFIELD(float, ambientIntensity)
    STRUCTFIELD(float, gt7PaperWhite)
    STRUCTFIELD(float, gt7ReferenceLuminance)
    STRUCTFIELD(uint, rtUseGlobalMaterialReflectance)
    STRUCTFIELD(float, rtGlobalMaterialReflectance)
    STRUCTFIELD(BindlessTextureHandle, skyboxTextureIdx)
STRUCTEND()

#ifndef __cplusplus
layout(scalar, set = SharedDataUBOSet, binding = SharedDataUBOBindingSlot) uniform SharedDataUBOBlock
{
    SharedDataUBO ubo;
};

vec4 ApplyFrameJitter(vec4 clipPos)
{
    vec2 jitterNdc = ubo.jitterOffset * 2.0 / ubo.renderResolution;
    jitterNdc.y *= -1;
    clipPos.xy += jitterNdc * clipPos.w;
    return clipPos;
}
#endif

#ifndef __cplusplus
mat3 AdjugateFromWorldMat(mat4 m)
{
    return mat3(cross(m[1].xyz, m[2].xyz),
        cross(m[2].xyz, m[0].xyz),
        cross(m[0].xyz, m[1].xyz));
}
#endif
#endif // SHADERS_SCENE_H
 
