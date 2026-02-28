#version 450 core
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
layout(location = 0) in VertexOut
{
    vec2 fragTexCoord;
    flat uint matIdx;
}
IN;
#define TransformSSBOSet 2
#include "../../Globals/PerObjectBuffers.h"
#include "../../Globals/Textures.h"
#include "../../Globals/Types.h"


void main() 
{
    Material mat = globalObjectDataSSBO.materials[IN.matIdx];
    vec4 fragTexSample = IsMaterialFlagSet(mat.flags, MATERIAL_FLAG_DIFFUSE_BIT)
                             ? vec4(texture(GlobalBindlessTextures[nonuniformEXT(mat.diffuseTexture)], IN.fragTexCoord))
                             : vec4(1.0);
    if (fragTexSample.a < 1.0)
        discard;
}