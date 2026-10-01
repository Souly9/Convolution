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
#include "../../Globals/Tonemapping.h"

layout(location = 0) in VertexOut
{
    vec2 fragTexCoord;
}
IN;

layout(location = 0) out vec4 outColor;

void main()
{
    vec2 texCoords = IN.fragTexCoord;

    if (ubo.debugViewMode == DEBUG_VIEW_MODE_MOTION_VECTORS)
    {
        // Current -> previous offset in render pixels, 0.5 gray is static
        vec2 velocityNdc = SamplePointClamp(gbufferUBO.gbufferVelocityIdx, texCoords).rg;
        vec2 toPrevPixels = velocityNdc * vec2(-0.5, 0.5) * ubo.renderResolution;
        outColor = vec4(0.5 + 0.5 * clamp(toPrevPixels / 16.0, vec2(-1.0), vec2(1.0)), 0.5, 1.0);
        return;
    }
    
    vec3 finalHDRColor = vec3(0.0);
    if ((ubo.debugFlags & DEBUG_FLAG_RT_DEBUG_ENABLED) != 0u)
    {
        finalHDRColor = SamplePointClamp(gbufferUBO.rtDebugViewIdx, texCoords).xyz;
    }
    else
    {
        finalHDRColor = SamplePointClamp(gbufferUBO.compositeInputIdx, texCoords).xyz;
    }

    if (gbufferUBO.bloomResultIdx != 0u)
    {
        vec3 bloomColor = SampleLinearClamp(gbufferUBO.bloomResultIdx, texCoords).rgb;
        finalHDRColor += bloomColor * 0.1;
    }

    finalHDRColor *= ubo.exposure;
 
     // Tone Mapping
    int toneMapperType = ubo.toneMapperType;
    vec3 finalLDRColor = finalHDRColor;

    if (toneMapperType == TONE_MAPPER_ACES)
        finalLDRColor = AcesTMO(finalHDRColor);
    else if (toneMapperType == TONE_MAPPER_UNCHARTED)
        finalLDRColor = Uncharted2TMO(finalHDRColor);
    else if (toneMapperType == TONE_MAPPER_GT7)
        finalLDRColor = GT7TMO(finalHDRColor);
    else
        finalLDRColor = clamp(finalHDRColor, 0.0, 1.0);


    vec3 finalSceneColor = pow(finalLDRColor, vec3(1.0 / 2.2));
    outColor = vec4(finalSceneColor, 1.0);
}
