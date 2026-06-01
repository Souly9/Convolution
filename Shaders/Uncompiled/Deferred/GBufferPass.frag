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
#include "../../Globals/MaterialHelpers.h"
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
    vec4 fragTexSample = SampleMaterialBaseColorTextureRGBA(mat, IN.fragTexCoord);

    vec3 N = normalize(IN.worldNormal);
    N = ApplyMaterialNormalMap(mat, IN.fragTexCoord, IN.TBN, N);

    vec2 velocity = ComputeVelocity(IN.unjitteredClipPos, IN.prevUnjitteredClipPos);

    StoreAlbedoInGBuffer(fragTexSample);
    StoreNormalAndMaterialInGBuffer(N, IN.matIdx);
    StoreTexCoordInGBuffer(IN.fragTexCoord);
    StoreVelocityInGBuffer(velocity);

    vec3 materialAlbedo = mat.baseColor.rgb * fragTexSample.rgb;
    SurfaceParameters surface = BuildMaterialSurface(mat, IN.fragTexCoord, materialAlbedo);
    outRoughness = surface.roughness;
}
