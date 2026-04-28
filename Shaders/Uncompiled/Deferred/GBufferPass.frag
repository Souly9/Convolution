#version 450 core
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

#define SharedDataUBOSet     1
#define TransformSSBOSet     2
#define PassPerObjectDataSet 3
#define GBufferUBOSet        4
#define TileArraySet         5

#include "../../Globals/Common.h"
#include "../../Globals/Scene.h"
#include "../../Globals/Bindless.h"
#include "../../Globals/DebugPrintf.h"
#include "../../Globals/GBuffer/Output.h"
#include "../../Globals/GeometryPassData.h"
#include "../../Globals/Utils/Math.h"

layout(location = 0) in VertexOut
{
    mat3 TBN;
    vec4 jitteredClipPos;
    vec4 unjitteredClipPos;
    vec4 prevUnjitteredClipPos;
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
            if (IsMaterialFlagSet(mat.flags, MATERIAL_FLAG_FLIPPED_NORMAL_BIT))
        {
                N.y = -N.y;
            }
        }
    }

    vec2 velocity = ComputeVelocity(IN.unjitteredClipPos, IN.prevUnjitteredClipPos);

    StoreAlbedoInGBuffer(fragTexSample);
    StoreNormalAndMaterialInGBuffer(N, IN.matIdx);
    StoreTexCoordInGBuffer(IN.fragTexCoord);
    StoreVelocityInGBuffer(velocity);
}
