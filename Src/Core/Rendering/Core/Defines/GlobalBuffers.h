#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Material.h"
#include "LightDefines.h"

namespace UBO
{

	// Contains all model matrices of the scene, mainly because we assume we will perform some kind of rendering operation on every entity in this renderer
	struct GlobalTransformSSBO
	{
		stltype::fixed_vector<DirectX::XMFLOAT4X4, MAX_ENTITIES, false> modelMatrices{};
	};
	// Contains all material data and so on
	struct GlobalObjectData
	{
		Material material;

	};
	struct GlobalObjectDataSSBO
	{
		MaterialBuffer materials{};
	};

	static constexpr u64 GlobalPerObjectDataSSBOSize = sizeof(GlobalObjectDataSSBO::materials);
	static constexpr u64 GlobalTransformSSBOSize = sizeof(GlobalTransformSSBO::modelMatrices);

	struct Tile
	{
		stltype::vector<RenderLight> lights{};
	};
	static constexpr u64 TILE_SIZE = sizeof(RenderLight) * MAX_LIGHTS_PER_TILE;
	// SSBO containing the tile array containing light data etc. used to shade the scene
	struct GlobalTileArraySSBO
	{
		stltype::fixed_vector<Tile, MAX_TILES, false> tiles{};
	};
	static constexpr u64 GlobalTileArraySSBOSize = TILE_SIZE * MAX_TILES;
	// SSBO containing the indices for objects rendered by a specific pass to access the global transforms, materials etc.
	struct PerPassObjectDataSSBO
	{
		stltype::fixed_vector<u32, MAX_ENTITIES, false> transformIdx;
		stltype::fixed_vector<u32, MAX_ENTITIES, false> perObjectDataIdx;
	};
	static constexpr u64 PerPassObjectDataSSBOSize = sizeof(u32) * 2 * MAX_ENTITIES;
	static constexpr u64 PerPassObjectDataTransformIdxOffset = 0;
	static constexpr u64 PerPassObjectDataPerObjectDataIdxOffset = sizeof(u32) * MAX_ENTITIES;

	struct ViewUBO
	{
		DirectX::XMFLOAT4X4 view;
		DirectX::XMFLOAT4X4 projection;
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
}