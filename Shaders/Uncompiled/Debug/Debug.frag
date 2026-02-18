#version 450 core
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_shading_language_include : enable

#define ViewUBOSet           1
#define TransformSSBOSet     2
#define PassPerObjectDataSet 3

#include "../../Globals/DrawBuildBuffers.h"
#include "../../Globals/GlobalBuffers.h"
#include "../../Globals/PerObjectBuffers.h"
#include "../../Globals/Textures.h"

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in flat uint transformIdx;

layout(location = 0) out vec4 outDebug;

void main()
{
    InstanceData instance = globalInstanceDataSSBO.instances[transformIdx];
    uint matIdx = uint(instance.aabbExtentsMatIdx.w);
    Material mat = globalObjectDataSSBO.materials[matIdx];

    vec4 finalColor = vec4(fragColor, 1.0);
    
    if (mat.diffuseTexture != 0)
    {
        vec4 texSample = texture(GlobalBindlessTextures[nonuniformEXT(mat.diffuseTexture)], fragTexCoord);
        finalColor.rgb *= texSample.rgb;
        if (texSample.a < 0.5) discard;
    }

    outDebug = finalColor;
}