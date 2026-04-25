#pragma once
#include "../AABB.h"
#include "../Material.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "LightDefines.h"
#include "../../../../Shaders/Globals/ClusteredShading.h"
#include "../../../../Shaders/Globals/GBuffer/GBufferSampling.h"
#include "../../../../Shaders/Globals/GeometryPassData.h"
#include "../../../../Shaders/Globals/Shadows/Data.h"

namespace UBO
{
using MaterialBuffer = stltype::vector<Material>;
using InstanceData = ::InstanceData;
using SharedDataUBO = ::SharedDataUBO;
using GBufferPostProcessUBO = ::GBufferPostProcessUBO;
using ShadowMapUBO = ::ShadowMapUBO;
using ShadowmapViewUBO = ::ShadowmapViewUBO;
using ClusterAABB = ::ClusterAABB;

// Contains all model matrices of the scene
struct GlobalTransformSSBO
{
    stltype::vector<mathstl::Matrix> modelMatrices{};
};

struct GlobalAABBSSBO
{
    stltype::vector<AABB> aabbs{};
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
static constexpr u64 GlobalPerObjectDataSSBOSize = sizeof(GlobalInstanceDataBuffer);
static constexpr u64 GlobalMaterialSSBOSize = sizeof(GlobalObjectDataBuffer);
static constexpr u64 GlobalTransformSSBOSize = sizeof(GlobalTransformBuffer);
static constexpr u64 GlobalAABBSSBOSize = MAX_ENTITIES * sizeof(AABB);

// All scene lights SSBO staging data
struct LightClusterSSBO
{
    DirectionalRenderLight dirLight;
    u32 numLights;
    stltype::vector<RenderLight> lights;

    // Cluster light indices (output from LightGridComputePass)
    stltype::vector<u32> clusterOffsets;      // +1 for end sentinel
    stltype::vector<u32> clusterLightIndices; // counts + light indices
};

static constexpr u64 LightClusterHeaderSize = offsetof(LightClusterBuffer, lights);
static constexpr u64 LightClusterLightsOffset = offsetof(LightClusterBuffer, lights);
static constexpr u64 LightClusterLightsSize = MAX_SCENE_LIGHTS * sizeof(RenderLight);
static constexpr u64 LightClusterOffsetsOffset = offsetof(LightClusterBuffer, clusterOffsets);
static constexpr u64 LightClusterOffsetsSize = sizeof(u32) * (MAX_CLUSTERS + 1);
static constexpr u64 LightClusterIndicesOffset = offsetof(LightClusterBuffer, clusterLightIndices);
static constexpr u64 LightClusterIndicesSize = sizeof(u32) * MAX_LIGHT_INDICES;
static constexpr u64 LightClusterSSBOSize = sizeof(LightClusterBuffer);

// Cluster grid bounds (generated from frustum)
struct ClusterAABBSet
{
    stltype::vector<ClusterAABB> clusters;
};
static constexpr u64 ClusterAABBSetSize = sizeof(ClusterGridBuffer);

// Compacted light indices output - offsets first, then count+indices per cluster
struct ClusterLightIndexSSBO
{
    stltype::vector<u32> offsets; // +1 for end sentinel
    stltype::vector<u32> indices; // counts + light indices
};
static constexpr u64 ClusterLightIndexSSBOSize = ((MAX_CLUSTERS + 1) + MAX_LIGHT_INDICES) * sizeof(u32);

struct ViewSpaceLightsSSBO
{
    stltype::vector<mathstl::Vector4> lights; // xyz=view-space position, w=radius
    stltype::vector<u32> tileCounters;
    stltype::vector<u32> tileLightIndices;
};
static constexpr u64 ViewSpaceLights_LightsSize = offsetof(ViewSpaceLightsBuffer, tileCounters);
static constexpr u64 ViewSpaceLights_TileCountersSize =
    offsetof(ViewSpaceLightsBuffer, tileLightIndices) - offsetof(ViewSpaceLightsBuffer, tileCounters);
static constexpr u64 ViewSpaceLights_TileIndicesSize =
    sizeof(ViewSpaceLightsBuffer) - offsetof(ViewSpaceLightsBuffer, tileLightIndices);
static constexpr u64 ViewSpaceLightsSSBOSize = sizeof(ViewSpaceLightsBuffer);

// SSBO containing the indices for objects rendered by a specific pass to access the global transforms, materials etc.
struct PerPassObjectDataSSBO
{
    stltype::vector<u32> instanceIndex;
};
static constexpr u64 PerPassObjectDataSSBOSize = sizeof(PerPassObjectBuffer);

struct RenderPassUBO
{
    stltype::vector<u32> entityIndices{};
};

static constexpr u64 ShadowmapViewUBOSize = sizeof(ShadowmapViewUBO);

} // namespace UBO
