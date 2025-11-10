#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#include "BindingSlots.h"

#define MAX_LIGHTS_PER_TILE 32
#define TileArraySet 3

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

struct Tile
{
	Light lights[MAX_LIGHTS_PER_TILE];
}; 
layout(scalar, set = TileArraySet, binding = GlobalTileArraySSBOSlot) readonly buffer GlobalTileSSBO
{
	DirectionalLight dirLight;
	Tile tiles[];
} lightTileArraySSBO;

struct LightUniforms
{
	vec4 CameraPos;
	vec4 LightGlobals; // ambient
};
layout(std140, set = TileArraySet, binding = GlobalLightDataUBOSlot) uniform LightUBO
{
	LightUniforms data;
} lightUniforms;