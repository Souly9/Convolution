uint GetClusterLightBaseIndex(uint clusterIdx)
{
    return lightData.clusterOffsets[clusterIdx];
}

uint GetClusterLightCount(uint baseIndex)
{
    return lightData.clusterLightIndices[baseIndex];
}

uint FetchClusterLightIndex(uint baseIndex, uint i)
{
    return lightData.clusterLightIndices[baseIndex + 1 + i];
}

void StoreClusterLightOffsetAndCount(uint clusterIdx, uint baseIndex, uint visibleCount)
{
    lightData.clusterOffsets[clusterIdx] = baseIndex;
    lightData.clusterLightIndices[baseIndex] = visibleCount;
}

void StoreClusterLightIndex(uint baseIndex, uint i, uint lightIdx)
{
    lightData.clusterLightIndices[baseIndex + 1 + i] = lightIdx;
}
