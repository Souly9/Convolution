#pragma once
#include "Core/../../Shaders/Globals/BindingSlots.h"

static constexpr u32 s_viewBindingSlot = ViewUBOBindingSlot;
static constexpr u32 s_modelSSBOBindingSlot = GlobalTransformDataSSBOSlot;
static constexpr u32 s_tileArrayBindingSlot = GlobalTileArraySSBOSlot;
static constexpr u32 s_perPassObjectDataBindingSlot = PassPerObjectDataSSBOSlot;
static constexpr u32 s_globalMaterialBufferSlot = GlobalObjectDataSSBOSlot;
static constexpr u32 s_globalLightUniformsBindingSlot = GlobalLightDataUBOSlot;
static constexpr u32 s_globalGbufferPostProcessUBOSlot = GlobalGBufferPostProcessUBOSlot;
static constexpr u32 s_globalInstanceDataSSBOSlot = GlobalInstanceDataSSBOSlot;
static constexpr u32 s_shadowmapUBOBindingSlot = GlobalShadowMapUBOSlot;
static constexpr u32 s_shadowmapViewUBOBindingSlot = ShadowMapDataBindingSlot;
static constexpr u32 s_clusterAABBsBindingSlot = ClusterGridSSBOSlot;
static constexpr u32 s_globalBindlessTextureBufferBindingSlot = GlobalBindlessTextureBufferSlot;
static constexpr u32 s_globalBindlessArrayTextureBufferBindingSlot = GlobalBindlessArrayTextureBufferSlot;
static constexpr u32 s_globalBindlessViewMatricesBufferBindingSlot = 300;
static constexpr u32 s_clusterGridSSBOBindingSlot = ClusterGridSSBOSlot;
