#include "Types.h"

vec4 DepthToWorldSpace(vec2 texCoords, float fragmentDepth, mat4 invViewProjection)
{
    vec4 clipSpacePosition;
    clipSpacePosition.x = texCoords.x * 2.0 - 1.0;
    clipSpacePosition.y = texCoords.y * 2.0 - 1.0;
    clipSpacePosition.y = -clipSpacePosition.y;
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

    vec2 currUV = currNDC * 0.5 + 0.5;
    vec2 prevUV = prevNDC * 0.5 + 0.5;
    
    currUV.y = - currUV.y;
    prevUV.y = - prevUV.y;

    return currUV - prevUV;
}
