#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#define GBufferUBOSet 4
#include "../../Globals/GBufferUBO.h"
#include "../../Globals/Textures.h"

layout(location = 0) in VertexOut
{
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
	vec2 texCoords = vec2(1.0 - IN.fragTexCoord.x, 1.0 - IN.fragTexCoord.y);
	outColor = vec4(texture(GlobalBindlessTextures[gbufferUBO.gbufferPositionIdx], texCoords).xyz, 1);
	vec4 uiColor = vec4(texture(GlobalBindlessTextures[gbufferUBO.gbufferUIIdx], texCoords));
	uiColor.a = mix(0, 0.8, uiColor.a);
	outColor = outColor * (1 - uiColor.a) + uiColor * uiColor.a;
}
