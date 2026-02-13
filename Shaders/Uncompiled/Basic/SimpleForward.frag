#version 450 core
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
#define ViewUBOSet 1
#define TransformSSBOSet 2
#define PassPerObjectDataSet 3
#include "../../Globals/GBufferOutput.h"
#include "../../Globals/GlobalBuffers.h"
#include "../../Globals/PerObjectBuffers.h"
#include "../../Globals/Textures.h"

layout(location = 0) in VertexOut
{
  vec4 worldPos;
  vec3 normal;
  vec2 fragTexCoord;
  flat uint matIdx;
} IN;


void main() {
   
  Material mat = globalObjectDataSSBO.materials[IN.matIdx];
	vec4 fragTexSample = vec4(texture(GlobalBindlessTextures[mat.diffuseTexture], IN.fragTexCoord));

	StoreAlbedoInGBuffer(vec4(fragTexSample.xyz, 1));
  StoreNormalAndMaterialInGBuffer(IN.normal, IN.matIdx);
}