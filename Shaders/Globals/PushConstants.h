#ifndef SHADERS_PUSH_CONSTANTS_H
#define SHADERS_PUSH_CONSTANTS_H

#include "Types.h"

STRUCTDECL(ClusterPushConstants)
    STRUCTFIELD(ivec3, clusterCount)
    STRUCTFIELD(uint, numLights)
    STRUCTFIELD(vec4, nearFar) // x=near, y=far, zw=unused
STRUCTEND()

STRUCTDECL(FrustumCullingPushConstants)
    STRUCTFIELD(uint, objectCount)
STRUCTEND()

STRUCTDECL(TAAPushConstants)
    STRUCTFIELD(uint, frameIndex)
    STRUCTFIELD(uint, resetHistory)
    STRUCTFIELD(float, resolutionX)
    STRUCTFIELD(float, resolutionY)
    STRUCTFIELD(float, zNear)
    STRUCTFIELD(float, zFar)
    STRUCTFIELD(float, velocityRejectionStart)
    STRUCTFIELD(float, velocityRejectionEnd)
    STRUCTFIELD(uint, debugMode)
    STRUCTFIELD(uint, forceHistory)
STRUCTEND()

STRUCTDECL(ScreenSpaceShadowPushConstants)
    STRUCTFIELD(vec4, lightCoordinate)
    STRUCTFIELD(ivec2, waveOffset)
    STRUCTFIELD(vec2, invDepthTextureSize)
    STRUCTFIELD(uint, depthTexIdx)
    STRUCTFIELD(uint, outputTexIdx)
STRUCTEND()

STRUCTDECL(SMAAPushConstants)
    STRUCTFIELD(vec4, metrics) // { 1/w, 1/h, w, h }
    STRUCTFIELD(uint, tex1)
    STRUCTFIELD(uint, tex2)
    STRUCTFIELD(uint, tex3)
STRUCTEND()

STRUCTDECL(RTDebugViewPushConstants)
    STRUCTFIELD(uint, outputTexIdx)
    STRUCTFIELD(uint, debugMode)
    STRUCTFIELD(float, maxRayDistance)
    STRUCTFIELD(float, pad0)
STRUCTEND()

STRUCTDECL(RTReflectionsPushConstants)
    STRUCTFIELD(uint, reflectionsTexIdx)
    STRUCTFIELD(uint, reflectedSceneColorTexIdx)
    STRUCTFIELD(uint, inputSceneColorTexIdx)
    STRUCTFIELD(uint, debugMode)
    STRUCTFIELD(float, maxRayDistance)
    STRUCTFIELD(float, reflectionIntensity)
    STRUCTFIELD(uint, hasReadyTLAS)
    STRUCTFIELD(uint, pad0)
STRUCTEND()

#endif // SHADERS_PUSH_CONSTANTS_H
