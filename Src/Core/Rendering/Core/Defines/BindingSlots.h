#pragma once
#include "Core/../../Shaders/Globals/BindingSlots.h"

static constexpr inline u32 s_viewBindingSlot = ViewUBOBindingSlot;
static constexpr inline u32 s_renderPassBindingSlot = RenderPassUBOSlot;
static constexpr inline u32 s_modelSSBOBindingSlot = GlobalTransformDataSSBOSlot;
static constexpr inline u32 s_tileArrayBindingSlot = GlobalTileArraySSBOSlot;