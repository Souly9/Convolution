#pragma once
#include "../Material.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "LightDefines.h"

namespace UBO
{
using MaterialBuffer = stltype::fixed_vector<Material, MAX_MATERIALS, false>;

// Contains all model matrices of the scene
struct GlobalTransformSSBO
{
    stltype::fixed_vector<mathstl::Matrix, MAX_ENTITIES, false> modelMatrices{};
};

// Can probably pack each float into half an uint or so but enough for now
struct InstanceData
{
    // Needed to construct the indirect draw cmd on GPU
    MeshResourceData drawData;
    mathstl::Vector4 aabbCenterTransIdx;
    mathstl::Vector4 aabbExtentsMatIdx;

    void SetTransformIdx(u32 idx)
    {
        aabbCenterTransIdx.w = (f32)idx;
    }

    void SetMaterialIdx(u32 idx)
    {
        aabbExtentsMatIdx.w = (f32)idx;
    }
};

struct GlobalInstanceSSBO
{
    stltype::fixed_vector<InstanceData, MAX_ENTITIES, false> instanceData{};
};
struct GlobalMaterialSSBO
{
    MaterialBuffer materials{};
};

static constexpr u64 GlobalPerObjectDataSSBOSize = sizeof(GlobalInstanceSSBO);
static constexpr u64 GlobalMaterialSSBOSize = sizeof(GlobalMaterialSSBO);
static constexpr u64 GlobalTransformSSBOSize = sizeof(GlobalTransformSSBO::modelMatrices);

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
    u32 _pad[3];
    stltype::array<RenderLight, MAX_SCENE_LIGHTS> lights;

    // Cluster light indices (output from LightGridComputePass)
    stltype::array<u32, MAX_CLUSTERS + 1> clusterOffsets;       // +1 for end sentinel
    stltype::array<u32, MAX_LIGHT_INDICES> clusterLightIndices; // counts + light indices
};

// Cluster grid bounds (generated from frustum)
struct ClusterAABBSet
{
    stltype::array<ClusterAABB, MAX_CLUSTERS> clusters;
};

// Compacted light indices output - offsets first, then count+indices per cluster
struct ClusterLightIndexSSBO
{
    stltype::array<u32, MAX_CLUSTERS + 1> offsets;  // +1 for end sentinel
    stltype::array<u32, MAX_LIGHT_INDICES> indices; // counts + light indices
};

static constexpr u64 LightClusterSSBOSize = sizeof(LightClusterSSBO);
static constexpr u64 ClusterAABBSetSize = sizeof(ClusterAABBSet);
static constexpr u64 ClusterLightIndexSSBOSize = sizeof(ClusterLightIndexSSBO);

// SSBO containing the indices for objects rendered by a specific pass to access the global transforms, materials etc.
struct PerPassObjectDataSSBO
{
    stltype::fixed_vector<u32, MAX_ENTITIES, false> instanceIndex;
};
static constexpr u64 PerPassObjectDataSSBOSize = sizeof(u32) * MAX_ENTITIES;

struct ViewUBO
{
    mathstl::Matrix view;
    mathstl::Matrix projection;
    mathstl::Matrix viewProjection;
    mathstl::Matrix viewInverse;
    mathstl::Matrix projectionInverse;
    mathstl::Vector4 viewPos;
};

struct RenderPassUBO
{
    stltype::vector<u32> entityIndices{};
};

struct GBufferPostProcessUBO
{
    BindlessTextureHandle gbufferPosition;
    BindlessTextureHandle gbufferNormal;
    BindlessTextureHandle gbuffer3;
    BindlessTextureHandle gbuffer4;
    BindlessTextureHandle gbufferUI;
    BindlessTextureHandle depthBufferIdx;
};

struct ShadowMapUBO
{
    BindlessTextureHandle directionalLightCSM;
};

struct ShadowmapViewUBO
{
    stltype::array<mathstl::Matrix, 16> lightViewProjMatrices{};
    stltype::array<mathstl::Vector4, 4> cascadeSplits{};
    s32 cascadeCount;
};
} // namespace UBO