#version 450 core
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#define SharedDataUBOSet     1
#define TransformSSBOSet     2
#define GBufferUBOSet        3

#include "../../Globals/Scene.h"
#include "../../Globals/Bindless.h"
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
    
    vec3 finalHDRColor = vec3(0.0);
    // 1 = TAA
    if (GET_AA_TYPE(ubo.debugFlags) == 1)
        finalHDRColor = texture(GlobalBindlessTextures[gbufferUBO.gbufferResolveIdx], texCoords).xyz;
    else
        finalHDRColor = texture(GlobalBindlessTextures[gbufferUBO.thisFrameColorBufferIdx], texCoords).xyz;

    // Tone Mapping
    int toneMapperType = ubo.toneMapperType;
    vec3 finalLDRColor = finalHDRColor;

    if (toneMapperType == 1)
        finalLDRColor = AcesTMO(finalHDRColor);
    else if (toneMapperType == 2)
        finalLDRColor = Uncharted2TMO(finalHDRColor);
    else
        finalLDRColor = GT7TMO(finalHDRColor);


    vec3 finalSceneColor = pow(finalLDRColor, vec3(1.0 / 2.2));
    outColor = vec4(finalSceneColor, 1.0);
}
