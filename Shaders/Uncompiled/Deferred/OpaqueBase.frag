#version 450 core
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_ARB_shading_language_include : enable
#define GBufferUBOSet 4
#include "../../Globals/GBufferUBO.h"
#include "../../Globals/Textures.h"

layout(location = 0) in VertexOut
{
  vec4 worldPos;
  vec2 fragTexCoord;
} IN;

layout(location = 0) out vec4 outColor;

void main() {
	//vec3 normal=IN.normal;
	//vec3 worldPos = IN.worldPos.xyz;
	//vec3 camPos = lightUniforms.data.CameraPos.xyz;
	//
	//Light lights[MAX_LIGHTS_PER_TILE] = lightTileArraySSBO.tiles[0].lights;
	//
	//Material mat = IN.mat;
	//vec3 diffuseCol=mat.baseColor.xyz;
	//// Use material roughness or custom set roughness 
	//float roughness= mat.roughness.x;
	//float metallic = mat.metallic.x;
	//
	//vec3 viewDir=normalize(camPos-worldPos.xyz);
	//
	//vec3 lighting = vec3(0);
   //// Direct Light loop
	//for(int i=0;i<MAX_LIGHTS_PER_TILE;++i)
	//{
	//	Light light=lights[i];
	//	vec3 lightPos=light.position.xyz;
	//	vec3 lightDir = normalize(lightPos - worldPos.xyz);
	//	vec3 halfwayDir = normalize(lightDir + viewDir);
	//
	//	vec3 l = computePointLight(worldPos.xyz, lightPos, viewDir, normal, halfwayDir, light.color.xyz, diffuseCol, roughness, metallic);
	//
	//	lighting.xyz += l;
	//}
	//
	////outColor = vec4(texture(GlobalBindlessTextures[mat.diffuseTexture], IN.fragTexCoord).rgb, 1);
	outColor = vec4(texture(GlobalBindlessTextures[gbufferUBO.gbufferPositionIdx], IN.fragTexCoord).xyz, 1);
}
