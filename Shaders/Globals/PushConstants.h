#ifndef SHADERS_PUSH_CONSTANTS_H
#define SHADERS_PUSH_CONSTANTS_H

#include "Types.h"

STRUCTDECL(ClusterPushConstants)
    STRUCTFIELD(ivec3, clusterCount)
    STRUCTFIELD(uint, numLights)
    STRUCTFIELD(vec4, nearFar) // x=near, y=far, zw=unused
STRUCTEND()

// View data (jitter, resolution, reset) comes from SharedDataUBO
STRUCTDECL(TAAPushConstants)
    STRUCTFIELD(float, velocityRejectionStart)
    STRUCTFIELD(float, velocityRejectionEnd)
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
    STRUCTFIELD(uint, debugMode)
    STRUCTFIELD(float, maxRayDistance)
    STRUCTFIELD(float, reflectionIntensity)
    STRUCTFIELD(uint, hasReadyTLAS)
    STRUCTFIELD(uint, frameIndex)
    STRUCTFIELD(uint, raysPerPixel)
STRUCTEND()

STRUCTDECL(RTAOPushConstants)
    STRUCTFIELD(uint, rtaoTexIdx)
    STRUCTFIELD(uint, hasReadyTLAS)
    STRUCTFIELD(uint, frameIndex)
    STRUCTFIELD(uint, raysPerPixel)
    STRUCTFIELD(float, aoRadius)
    STRUCTFIELD(float, aoIntensity)
STRUCTEND()

STRUCTDECL(RTCompositePushConstants)
    STRUCTFIELD(uint, resetHistory)
    STRUCTFIELD(float, accumRate)
    STRUCTFIELD(uint, accumTexIdx)
    STRUCTFIELD(uint, historyAccumTexIdx)
    STRUCTFIELD(uint, rtaoTexIdx)
    STRUCTFIELD(uint, rtReflectionsTexIdx)
STRUCTEND()

STRUCTDECL(BloomPushConstants)
    STRUCTFIELD(float, threshold)
    STRUCTFIELD(float, intensity)
    STRUCTFIELD(float, filterRadius)
    STRUCTFIELD(uint, useKarisAverage)
    STRUCTFIELD(uint, width)
    STRUCTFIELD(uint, height)
    STRUCTFIELD(uint, outputWidth)
    STRUCTFIELD(uint, outputHeight)
    STRUCTFIELD(uint, inputTexIdx)
    STRUCTFIELD(uint, inputTargetTexIdx)
    STRUCTFIELD(uint, outputImageIdx)
    STRUCTFIELD(uint, lensTextureIdx)
    STRUCTFIELD(uint, useLensTexture)
    STRUCTFIELD(float, lensDirtIntensity)
STRUCTEND()

#endif // SHADERS_PUSH_CONSTANTS_H
