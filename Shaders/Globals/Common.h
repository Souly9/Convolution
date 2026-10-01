#ifndef SHADERS_COMMON_H
#define SHADERS_COMMON_H

#include "Types.h"
#define MAX_MATERIALS          2048
#define MAX_ENTITIES           65536 * 3
#define MAX_CASCADE_COUNT      16
#define MAX_LIGHTS_PER_CLUSTER 128 // Fine culling limit (eg. max number of lights to eval)
#define MAX_SCENE_LIGHTS       16384
#define MAX_CLUSTERS           (32 * 32 * 32)
#define MAX_LIGHT_INDICES      (MAX_CLUSTERS * MAX_LIGHTS_PER_CLUSTER)
#define MAX_TILE_XY            (64 * 36)
#define MAX_LIGHTS_PER_TILE    512 // Coarse culling limit

#define SharedDataUBOBindingSlot             300
#define ShadowMapDataBindingSlot             301
#define GlobalBindlessTextureBufferSlot      1
#define GlobalBindlessArrayTextureBufferSlot 2
#define GlobalBindlessImageBufferSlot        3
#define GlobalSamplerSlot                    5

// Global sampler table, the bindless arrays hold textures only
#define SAMPLER_LINEAR_CLAMP  0
#define SAMPLER_POINT_CLAMP   1
#define SAMPLER_LINEAR_REPEAT 2 // Trilinear + anisotropic, material textures and the skybox
#define SAMPLER_SHADOW        3 // Linear, clamp to a black border (the far plane with reversed Z)
#define GLOBAL_SAMPLER_COUNT  4
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
#define GBufferUBOSet 3
#endif
#ifndef PassPerObjectDataSet
#define PassPerObjectDataSet 3
#endif
#ifndef TileArraySet
#define TileArraySet 4
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
#define DEBUG_FLAG_CULL_FRUSTUM            (1 << 7)
#define DEBUG_FLAG_RTAO_ENABLED            (1 << 8)
#define DEBUG_FLAG_DISABLE_CLUSTER_CULLING (1 << 16)

#define DEBUG_VIEW_MODE_NONE         0
#define DEBUG_VIEW_MODE_CSM_CASCADES 1
#define DEBUG_VIEW_MODE_CLUSTERS     2
#define DEBUG_VIEW_MODE_MOTION_VECTORS 3

#define RT_DEBUG_VIEW_MODE_NONE             0u
#define RT_DEBUG_VIEW_MODE_TLAS             1u
#define RT_DEBUG_VIEW_MODE_REFLECTIONS_ONLY 2u

#define TONE_MAPPER_NONE      0
#define TONE_MAPPER_ACES      1
#define TONE_MAPPER_UNCHARTED 2
#define TONE_MAPPER_GT7       3

#define RT_REFLECTION_DEBUG_NONE             0u
#define RT_REFLECTION_DEBUG_REFLECTIONS_ONLY 1u

#endif // SHADERS_COMMON_H
