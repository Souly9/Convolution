#include "BindingSlots.h"
#include "Types.h"
layout(set = GBufferUBOSet, binding = GlobalGBufferPostProcessUBOSlot) uniform GBufferUBO
{
    uint gbufferAlbedoIdx;
    uint gbufferNormalIdx;
    uint gbufferTexCoordMatIdx;
    uint gbufferDebugIdx;
    uint depthBufferIdx;
} gbufferUBO;

layout(set = GBufferUBOSet, binding = GlobalShadowMapUBOSlot) uniform ShadowMapUBO
{
    uint directionalShadowMapIdx;
} shadowmapUBO;