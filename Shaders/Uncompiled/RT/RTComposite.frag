#version 450 core
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#define SharedDataUBOSet     1
#define TransformSSBOSet     2
#define GBufferUBOSet        3

#include "../../Globals/Scene.h"
#include "../../Globals/Bindless.h"
#include "../../Globals/Common.h"
#include "../../Globals/GBuffer/GBufferSampling.h"

layout(location = 0) in VertexOut
{
    vec2 fragTexCoord;
}
IN;

layout(location = 0) out vec4 outColor;

void main()
{
    vec2 uv = IN.fragTexCoord;

    // 1. Sample deferred lighting color (original output from LightingPass)
    vec3 sceneColor = texture(GlobalBindlessTextures[nonuniformEXT(gbufferUBO.deferredLightingColorIdx)], uv).rgb;

    // 2. Apply Ambient Occlusion (if enabled)
    if ((ubo.debugFlags & DEBUG_FLAG_RT_ENABLED) != 0u && 
        (ubo.debugFlags & DEBUG_FLAG_RTAO_ENABLED) != 0u && 
        gbufferUBO.rtaoIdx != 0u)
    {
        float ao = texture(GlobalBindlessTextures[nonuniformEXT(gbufferUBO.rtaoIdx)], uv).r;
        sceneColor *= ao;
    }

    // 3. Add RT Reflections (if enabled)
    if ((ubo.debugFlags & DEBUG_FLAG_RT_ENABLED) != 0u && 
        (ubo.debugFlags & DEBUG_FLAG_RT_REFLECTIONS_ENABLED) != 0u && 
        gbufferUBO.rtReflectionsIdx != 0u)
    {
        vec3 reflections = texture(GlobalBindlessTextures[nonuniformEXT(gbufferUBO.rtReflectionsIdx)], uv).rgb;
        sceneColor += reflections;
    }

    outColor = vec4(sceneColor, 1.0);
}
