#ifndef MATH_H
#define MATH_H
#include "../Types.h"

#ifndef __cplusplus
vec4 DepthToWorldSpace(vec2 texCoords, float fragmentDepth, mat4 invViewProjection)
{
    vec4 clipSpacePosition;
    clipSpacePosition.x = texCoords.x * 2.0 - 1.0;
    clipSpacePosition.y = 1.0 - texCoords.y * 2.0;
    clipSpacePosition.xy -= FrameJitterNdc();
    clipSpacePosition.z = fragmentDepth;
    clipSpacePosition.w = 1.0;

    vec4 worldSpacePosition = invViewProjection * clipSpacePosition;
    return worldSpacePosition / worldSpacePosition.w;
}

FUNC_QUALIFIER vec2 ClipToNDC(vec4 clipPos)
{
    return clipPos.xy / clipPos.w;
}

FUNC_QUALIFIER vec2 ComputeVelocity(vec4 currClipPos, vec4 prevClipPos)
{
    vec2 currNDC = ClipToNDC(currClipPos);
    vec2 prevNDC = ClipToNDC(prevClipPos);

    return currNDC - prevNDC;
}
#endif

#endif // MATH_H