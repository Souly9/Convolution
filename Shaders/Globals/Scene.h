#ifndef SHADERS_SCENE_H
#define SHADERS_SCENE_H
#extension GL_EXT_scalar_block_layout : enable

#include "Common.h"
#include "Material.h"

struct IndexedIndirectDrawCmd
{
    uint    indexCount;
    uint    instanceCount;
    uint    firstIndex;
    int    vertexOffset;
    uint    firstInstance;
};

struct MeshResourceData
{
    uint vertBufferOffset;
    uint indexBufferOffset;
    uint vertCount;
    uint indexCount;
};

struct InstanceData
{
    MeshResourceData drawData;
    vec4 aabbCenterTransIdx;
    vec4 aabbExtentsMatIdx;
    uint flags;
};

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

struct Light
{
    vec4 position;
    vec4 direction;
    vec4 color;
    vec4 cutoff;
};

struct DirectionalLight
{
    vec4 direction;
    vec4 color;
};

layout(scalar, set = SharedDataUBOSet, binding = SharedDataUBOBindingSlot) uniform SharedDataUBO
{
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 viewProjectionInverse;
    mat4 viewInverse;
    mat4 projectionInverse;
    mat4 jitteredProjection;
    mat4 jitteredViewProjectionInverse;
    mat4 prevView;
    mat4 prevProjection;
    mat4 prevViewProjection;
    vec4 viewPos;

    // Debug & Global settings
    uint debugFlags;
    int debugViewMode;
    float exposure;
    int toneMapperType;
    float ambientIntensity;
    float gt7PaperWhite;
    float gt7ReferenceLuminance;
}
ubo;

mat3 AdjugateFromWorldMat(mat4 m)
{
    return mat3(cross(m[1].xyz, m[2].xyz),
        cross(m[2].xyz, m[0].xyz),
        cross(m[0].xyz, m[1].xyz));
}
#endif // SHADERS_SCENE_H
 


