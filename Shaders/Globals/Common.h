#ifndef SHADERS_COMMON_H
#define SHADERS_COMMON_H

#include "Types.h"
#define MAX_MATERIALS          2048
#define MAX_ENTITIES           65536 * 3
#define MAX_CASCADE_COUNT      16
#define MAX_LIGHTS_PER_CLUSTER  12
#define MAX_SCENE_LIGHTS       16384 * 5
#define MAX_CLUSTERS           (64 * 64 * 64)
#define MAX_LIGHT_INDICES      (MAX_CLUSTERS * MAX_LIGHTS_PER_CLUSTER)
#define MAX_TILE_XY            (64 * 64)
#define MAX_LIGHTS_PER_TILE    4096

#define SharedDataUBOBindingSlot             300
#define ShadowMapDataBindingSlot             301
#define GlobalBindlessTextureBufferSlot      1
#define GlobalBindlessArrayTextureBufferSlot 2
#define GlobalBindlessImageBufferSlot        3
#define GlobalTileArraySSBOSlot              1
#define GlobalLightDataUBOSlot               2
#define GlobalViewSpaceLightsSSBOSlot        3

#define GlobalPerObjectDataSSBOSlot          502
#define PassPerObjectDataSSBOSlot            1
#define ClusterGridSSBOSlot                  1

#define GlobalTransformDataSSBOSlot 1
#define GlobalObjectDataSSBOSlot    2
#define GlobalInstanceDataSSBOSlot  3
#define SceneAABBsSSBOSlot          4
#define PrevGlobalTransformDataSSBOSlot 5

#define GlobalGBufferPostProcessUBOSlot 1
#define GlobalShadowMapUBOSlot          2
#define RTSceneASBindingSlot            1

#define DEBUG_FLAG_SHADOWS_ENABLED (1 << 0)
#define DEBUG_FLAG_SSS_ENABLED     (1 << 1)
#define DEBUG_FLAG_RT_DEBUG_ENABLED (1 << 2)

#define GET_AA_TYPE(flags) ((flags >> 8) & 0xFF)

#endif // SHADERS_COMMON_H
