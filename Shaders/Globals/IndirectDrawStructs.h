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
    // Needed to construct the indirect draw cmd on GPU
    MeshResourceData drawData;
    vec4 aabbCenterTransIdx;
    vec4 aabbExtentsMatIdx;

};
// Standalone functions to access the data
uint GetTransformIdx(InstanceData data)
{
    return uint(data.aabbCenterTransIdx.w + 0.5);
}

uint GetMaterialIdx(InstanceData data)
{
    return uint(data.aabbExtentsMatIdx.w + 0.5);
}