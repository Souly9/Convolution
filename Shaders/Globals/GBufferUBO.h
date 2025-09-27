#include "GBuffer.h"
#include "BindingSlots.h"
layout(set = GBufferUBOSet, binding = GlobalGBufferPostProcessUBOSlot) uniform GBufferUBO
{
    uint gbufferPositionIdx;
    uint gbufferNormalIdx;
    uint gbuffer3Idx;
    uint gbuffer4Idx;
    uint gbufferUIIdx;
} gbufferUBO;