#pragma once
#include "Core/Global/GlobalDefines.h"
#include "LightDefines.h"

namespace UBO
{
	static constexpr u64 GlobalTransformSSBOSize = sizeof(DirectX::XMFLOAT4X4) * MAX_ENTITIES;

	// Contains all model matrices of the scene, mainly because we assume we will perform some kind of rendering operation on every entity in this renderer
	struct GlobalTransformSSBO
	{
		stltype::vector<DirectX::XMFLOAT4X4> modelMatrices{};
	};

	struct Tile
	{
		stltype::vector<RenderLight> lights{};
	};
	static constexpr u64 TILE_SIZE = sizeof(RenderLight) * MAX_LIGHTS_PER_TILE;
	static constexpr u64 GlobalTileArraySSBOSize = TILE_SIZE * 1;
	// SSBO containing the tile array containing light data etc. used to shade the scene
	struct GlobalTileArraySSBO
	{
		stltype::vector<Tile> tiles{};
	};

	struct ViewUBO
	{
		DirectX::XMFLOAT4X4 view;
		DirectX::XMFLOAT4X4 projection;
	};

	struct RenderPassUBO
	{
		stltype::vector<u32> entityIndices{};
	};
}