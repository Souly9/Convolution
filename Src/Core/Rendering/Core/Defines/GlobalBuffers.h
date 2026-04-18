#pragma once
#include "../Material.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "LightDefines.h"
#include "../../../../Shaders/Globals/Common.h"

namespace UBO
{
using MaterialBuffer = stltype::vector<Material>;

// Contains all model matrices of the scene
struct GlobalTransformSSBO
{
    stltype::vector<mathstl::Matrix> modelMatrices{};
};

struct GlobalAABBSSBO
{
    stltype::vector<AABB> aabbs{};
};

// Can probably pack each float into half an uint or so but enough for now
struct InstanceData
{
    // Needed to construct the indirect draw cmd on GPU
    MeshResourceData drawData;
    mathstl::Vector4 aabbCenterTransIdx;
    mathstl::Vector4 aabbExtentsMatIdx;
    u32 flags;

    void SetTransformIdx(u32 idx)
    {
        aabbCenterTransIdx.w = (f32)idx;
    }

    void SetMaterialIdx(u32 idx)
    {
        aabbExtentsMatIdx.w = (f32)idx;
    }

    void SetVisible(bool visible)
    {
        flags = visible ? 1 : 0;
    }
};

struct GlobalInstanceSSBO
{
    stltype::vector<InstanceData> instanceData{};
};
struct GlobalMaterialSSBO
{
    MaterialBuffer materials{};
};

// Explicit sizes for SSBOs since we can't use sizeof() with vectors
static constexpr u64 GlobalPerObjectDataSSBOSize = MAX_ENTITIES * sizeof(InstanceData);
static constexpr u64 GlobalMaterialSSBOSize = MAX_MATERIALS * sizeof(Material);
static constexpr u64 GlobalTransformSSBOSize = MAX_ENTITIES * sizeof(mathstl::Matrix);
static constexpr u64 GlobalAABBSSBOSize = MAX_ENTITIES * sizeof(AABB);

// Cluster bounds for intersection testing
struct ClusterAABB
{
    mathstl::Vector3 minBounds; // xyz=min
    mathstl::Vector3 maxBounds; // xyz=max
};

// All scene lights SSBO
struct LightClusterSSBO
{
    DirectionalRenderLight dirLight;
    u32 numLights;
    stltype::vector<RenderLight> lights;

    // Cluster light indices (output from LightGridComputePass)
    stltype::vector<u32> clusterOffsets;       // +1 for end sentinel
    stltype::vector<u32> clusterLightIndices; // counts + light indices
};

// Explicit sizes and offsets for LightClusterSSBO
static constexpr u64 LightClusterHeaderSize = sizeof(DirectionalRenderLight) + 4 + 12; // 48
static constexpr u64 LightClusterLightsOffset = LightClusterHeaderSize;
static constexpr u64 LightClusterLightsSize = MAX_SCENE_LIGHTS * sizeof(RenderLight);
static constexpr u64 LightClusterOffsetsOffset = LightClusterLightsOffset + LightClusterLightsSize;
static constexpr u64 LightClusterOffsetsSize = (MAX_CLUSTERS + 1) * sizeof(u32);
static constexpr u64 LightClusterIndicesOffset = LightClusterOffsetsOffset + LightClusterOffsetsSize;
static constexpr u64 LightClusterIndicesSize = MAX_LIGHT_INDICES * sizeof(u32);
static constexpr u64 LightClusterSSBOSize = LightClusterIndicesOffset + LightClusterIndicesSize;

// Cluster grid bounds (generated from frustum)
struct ClusterAABBSet
{
    stltype::vector<ClusterAABB> clusters;
};
static constexpr u64 ClusterAABBSetSize = MAX_CLUSTERS * sizeof(ClusterAABB);

// Compacted light indices output - offsets first, then count+indices per cluster
struct ClusterLightIndexSSBO
{
    stltype::vector<u32> offsets;  // +1 for end sentinel
    stltype::vector<u32> indices; // counts + light indices
};
static constexpr u64 ClusterLightIndexSSBOSize = ((MAX_CLUSTERS + 1) + MAX_LIGHT_INDICES) * sizeof(u32);

struct ViewSpaceLightsSSBO
{
    stltype::vector<mathstl::Vector4> lights; // xyz=view-space position, w=radius
    stltype::vector<u32> tileCounters;
    stltype::vector<u32> tileLightIndices;
};
static constexpr u64 ViewSpaceLights_LightsSize = MAX_SCENE_LIGHTS * sizeof(mathstl::Vector4);
static constexpr u64 ViewSpaceLights_TileCountersSize = MAX_TILE_XY * sizeof(u32);
static constexpr u64 ViewSpaceLights_TileIndicesSize = MAX_TILE_XY * MAX_LIGHTS_PER_TILE * sizeof(u32);
static constexpr u64 ViewSpaceLightsSSBOSize = ViewSpaceLights_LightsSize + ViewSpaceLights_TileCountersSize + ViewSpaceLights_TileIndicesSize;

// SSBO containing the indices for objects rendered by a specific pass to access the global transforms, materials etc.
struct PerPassObjectDataSSBO
{
    stltype::vector<u32> instanceIndex;
};
static constexpr u64 PerPassObjectDataSSBOSize = sizeof(u32) * MAX_ENTITIES;

struct SharedDataUBO
{
    mathstl::Matrix view;
    mathstl::Matrix projection;
    mathstl::Matrix viewProjection;
    mathstl::Matrix viewProjectionInverse;
    mathstl::Matrix viewInverse;
    mathstl::Matrix projectionInverse;
    mathstl::Matrix jitteredProjection;
    mathstl::Matrix jitteredViewProjectionInverse;
    mathstl::Matrix prevView;
    mathstl::Matrix prevProjection;
    mathstl::Matrix prevViewProjection;
    mathstl::Vector4 viewPos;

    // Debug & Global settings
    u32 debugFlags;
    s32 debugViewMode;
    f32 exposure;
    s32 toneMapperType;
    f32 ambientIntensity;
    f32 gt7PaperWhite;
    f32 gt7ReferenceLuminance;
};

struct RenderPassUBO
{
    stltype::vector<u32> entityIndices{};
};

struct GBufferPostProcessUBO
{
    BindlessTextureHandle gbufferAlbedo;
    BindlessTextureHandle gbufferNormal;
    BindlessTextureHandle gbufferTexCoordMat;
    BindlessTextureHandle gbufferDebug;
    BindlessTextureHandle gbufferVelocity;
    BindlessTextureHandle depthBufferIdx;
    BindlessTextureHandle lastFrameColorBufferIdx;
    BindlessTextureHandle thisFrameColorBufferIdx;
    BindlessTextureHandle lastFrameDepthIdx;
    BindlessTextureHandle gbufferResolve;
};

struct ShadowMapUBO
{
    BindlessTextureHandle directionalLightCSM;
};

struct ShadowmapViewUBO
{
    mathstl::Matrix lightViewProjMatrices[16]{};
    mathstl::Vector4 cascadeSplits[4]{};
    s32 cascadeCount{};
    BindlessTextureHandle screenSpaceShadows{};
};

static constexpr u64 ShadowmapViewUBOSize = sizeof(ShadowmapViewUBO);

} // namespace UBO
