#version 450 core
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
#include "../../Globals/GBufferPass.h"
#include "../../Globals/GBufferOutput.h"

layout(location = 0) in VertexOut
{
    mat3 TBN;
    vec4 worldPos;
    vec3 worldNormal;
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
    vec3 N = normalize(IN.worldNormal);
    if (IsMaterialFlagSet(mat.flags, MATERIAL_FLAG_NORMAL_BIT))
    {
        vec3 tangentNormal =
            texture(GlobalBindlessTextures[nonuniformEXT(mat.normalTexture)], IN.fragTexCoord).xyz * 2.0 - 1.0;
        if (dot(tangentNormal, tangentNormal) > 1e-6)
        {
            N = normalize(IN.TBN * tangentNormal);
            if (IsFlagSet(mat.flags, MATERIAL_FLAG_FLIPPED_NORMAL_BIT))
            {
                N.y = -N.y;
            }
        }
    }

    StoreAlbedoInGBuffer(fragTexSample);
    StoreNormalAndMaterialInGBuffer(N, IN.matIdx);
    StoreTexCoordInGBuffer(IN.fragTexCoord);
}
