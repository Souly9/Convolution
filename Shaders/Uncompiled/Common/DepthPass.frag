#version 450
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#define SharedDataUBOSet     1
#define TransformSSBOSet     2
#define PassPerObjectDataSet 3

#include "../../Globals/Common.h"
#include "../../Globals/Scene.h"
#include "../../Globals/Bindless.h"
#include "../../Globals/GeometryPassData.h"

layout(location = 0) in VertexOut
{
    vec2 fragTexCoord;
    flat uint matIdx;
}
IN;

void main()
{
    Material mat = globalObjectDataSSBO.materials[IN.matIdx];
    vec4 fragTexSample = IsMaterialFlagSet(mat.flags, MATERIAL_FLAG_DIFFUSE_BIT)
                             ? vec4(texture(GlobalBindlessTextures[nonuniformEXT(mat.diffuseTexture)], IN.fragTexCoord))
                             : vec4(1.0);
    // Dumb alpha discard
    if (fragTexSample.a < 1e-6)
        discard;
}