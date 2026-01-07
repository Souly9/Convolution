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

struct Tile
{
    stltype::vector<RenderLight> lights{};
};
static constexpr u64 TILE_SIZE = sizeof(RenderLight) * MAX_LIGHTS_PER_TILE;

// SSBO containing the tile array containing light data etc. used to shade the scene
struct GlobalTileArraySSBO
{
    DirectionalRenderLight dirLight;
    stltype::fixed_vector<Tile, MAX_TILES, false> tiles{};
};
static constexpr u64 GlobalTileArraySSBOSize = sizeof(struct GlobalTileArraySSBO);
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
    // Not valid
    // BindlessTextureHandle depthTexture;
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
    f32 _pad[3];
};
} // namespace UBO