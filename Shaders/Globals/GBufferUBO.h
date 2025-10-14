#include "GBuffer.h"
#include "BindingSlots.h"
layout(set = GBufferUBOSet, binding = GlobalGBufferPostProcessUBOSlot) uniform GBufferUBO
{
    uint gbufferAlbedoIdx;
    uint gbufferNormalIdx;
    uint gbufferTexCoordMatIdx;
    uint gbufferPositionIdx;
    uint gbufferUIIdx;
} gbufferUBO;