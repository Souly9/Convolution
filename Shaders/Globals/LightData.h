#extension GL_EXT_nonuniform_qualifier : enable
#include "BindingSlots.h"

#define MAX_LIGHTS_PER_TILE 32

struct Light
{
	vec4 position;
	vec4 direction;
	vec4 color;
	vec4 cutoff;
};
struct Tile
{
	Light lights[MAX_LIGHTS_PER_TILE];
};
layout(std140, set = 3, binding = GlobalTileArraySSBOSlot) readonly buffer GlobalTileSSBO
{
	Tile tiles[];
} lightTileArraySSBO;