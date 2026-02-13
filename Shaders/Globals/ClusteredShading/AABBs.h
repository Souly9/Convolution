#extension GL_EXT_scalar_block_layout : require

struct ClusterAABB
{
    vec4 minBounds;
    vec4 maxBounds;
};
// ClusterGrid (Set 0, Binding 1) - Pass local
layout(scalar, set = ClusterGridSet, binding = 1) buffer ClusterGrid
{
    ClusterAABB clusters[];
}
clusterGrid;