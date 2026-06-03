#ifndef SHADERS_COMMON_H
#define SHADERS_COMMON_H

#include "Types.h"
#define MAX_MATERIALS          2048
#define MAX_ENTITIES           65536 * 3
#define MAX_CASCADE_COUNT      16
#define MAX_LIGHTS_PER_CLUSTER 512 // Fine culling limit (eg. max number of lights to eval)
#define MAX_SCENE_LIGHTS       100000
#define MAX_CLUSTERS           (64 * 64 * 64)
#define MAX_LIGHT_INDICES      (MAX_CLUSTERS * MAX_LIGHTS_PER_CLUSTER)
#define MAX_TILE_XY            (64 * 64)
#define MAX_LIGHTS_PER_TILE    4096 // Coarse culling limit

#define SharedDataUBOBindingSlot             300
#define ShadowMapDataBindingSlot             301
#define GlobalBindlessTextureBufferSlot      1
#define GlobalBindlessArrayTextureBufferSlot 2
#define GlobalBindlessImageBufferSlot        3
#define GlobalTileArraySSBOSlot              1
#define GlobalLightDataUBOSlot               2
#define GlobalViewSpaceLightsSSBOSlot        3

#define GlobalPerObjectDataSSBOSlot 502
#define PassPerObjectDataSSBOSlot   1
#define ClusterGridSSBOSlot         1

#define GlobalTransformDataSSBOSlot     1
#define GlobalObjectDataSSBOSlot        2
#define GlobalInstanceDataSSBOSlot      3
#define SceneAABBsSSBOSlot              4
#define PrevGlobalTransformDataSSBOSlot 5

#define GlobalGBufferPostProcessUBOSlot 1
#define GlobalShadowMapUBOSlot          2
#define RTSceneASBindingSlot            1
#define RTInstanceHitDataBindingSlot    2
#define RTSceneVertexBufferBindingSlot  3
#define RTSceneIndexBufferBindingSlot   4

// Canonical descriptor set indices (C++ and shader side must agree)
#ifndef BindlessSet
#define BindlessSet 0
#endif
#ifndef SharedDataUBOSet
#define SharedDataUBOSet 1
#endif
#ifndef TransformSSBOSet
#define TransformSSBOSet 2
#endif
#ifndef GBufferUBOSet
#define GBufferUBOSet 4
#endif
#ifndef PassPerObjectDataSet
#define PassPerObjectDataSet 3
#endif
#ifndef TileArraySet
#define TileArraySet 3
#endif
#ifndef RTSceneASSet
#define RTSceneASSet 5
#endif

#define DEBUG_FLAG_SHADOWS_ENABLED         (1 << 0)
#define DEBUG_FLAG_SSS_ENABLED             (1 << 1)
#define DEBUG_FLAG_RT_DEBUG_ENABLED        (1 << 2)
#define DEBUG_FLAG_RT_ENABLED              (1 << 3)
#define DEBUG_FLAG_RT_REFLECTIONS_ENABLED  (1 << 4)
#define DEBUG_FLAG_SHOW_CLUSTER_AABBS      (1 << 5)
#define DEBUG_FLAG_TAA_FORCE_HISTORY       (1 << 6)
#define DEBUG_FLAG_CULL_FRUSTUM            (1 << 7)
#define DEBUG_FLAG_RTAO_ENABLED            (1 << 8)
#define DEBUG_FLAG_DISABLE_CLUSTER_CULLING (1 << 16)

#define DEBUG_VIEW_MODE_NONE         0
#define DEBUG_VIEW_MODE_CSM_CASCADES 1
#define DEBUG_VIEW_MODE_CLUSTERS     2

#define TAA_DEBUG_MODE_OFF                        0u
#define TAA_DEBUG_MODE_CURRENT_COLOR              1u
#define TAA_DEBUG_MODE_HISTORY_COLOR              2u
#define TAA_DEBUG_MODE_HISTORY_CURRENT_DIFF       3u
#define TAA_DEBUG_MODE_VELOCITY_MAGNITUDE         4u
#define TAA_DEBUG_MODE_HISTORY_VELOCITY_MAGNITUDE 5u

#define AA_TYPE_TAA_SMAA 1u
#define AA_TYPE_SMAA     2u
#define AA_TYPE_DLSS     3u
#define AA_TYPE_XESS     4u

#define RT_DEBUG_VIEW_MODE_NONE             0u
#define RT_DEBUG_VIEW_MODE_TLAS             1u
#define RT_DEBUG_VIEW_MODE_REFLECTIONS_ONLY 2u

#define TONE_MAPPER_NONE      0
#define TONE_MAPPER_ACES      1
#define TONE_MAPPER_UNCHARTED 2
#define TONE_MAPPER_GT7       3

#define RT_REFLECTION_DEBUG_NONE             0u
#define RT_REFLECTION_DEBUG_REFLECTIONS_ONLY 1u

#define GET_AA_TYPE(flags) ((flags >> 8) & 0xFF)

#endif // SHADERS_COMMON_H
