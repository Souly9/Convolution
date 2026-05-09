#ifndef SHADERS_UTILS_SCREEN_SPACE_H
#define SHADERS_UTILS_SCREEN_SPACE_H

#include "../Types.h"

#ifndef __cplusplus
FUNC_QUALIFIER bool IsUVInBounds(vec2 uv)
{
    return all(greaterThanEqual(uv, vec2(0.0))) && all(lessThanEqual(uv, vec2(1.0)));
}

FUNC_QUALIFIER vec2 ProjectWorldToUV(vec3 worldPos, mat4 viewProjection)
{
    vec4 clip = viewProjection * vec4(worldPos, 1.0);
    vec2 uv = (clip.xy / clip.w) * 0.5 + 0.5;
    uv.y = 1.0 - uv.y;
    return uv;
}
#endif

#endif // SHADERS_UTILS_SCREEN_SPACE_H
