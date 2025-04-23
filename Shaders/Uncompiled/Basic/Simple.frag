#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#define ViewUBOSet 1
#include "../../Globals/LightData.h"
#include "../../Globals/Material.h"
#include "../../Globals/PBR/UnrealPBR.h"

layout(set = 0, binding = GlobalBindlessTextureBufferSlot) uniform sampler2D GlobalBindlessTextures[];

layout(location = 0) in VertexOut
{
  vec4 worldPos;
  vec3 normal;
  vec2 fragTexCoord;
  Material mat;
   } IN;

layout(location = 0) out vec4 outColor;

void main() {

	vec3 normal=IN.normal;
	vec3 worldPos = IN.worldPos.xyz;
	vec3 camPos = lightUniforms.data.CameraPos.xyz;

	Light lights[MAX_LIGHTS_PER_TILE] = lightTileArraySSBO.tiles[0].lights;

	Material mat = IN.mat;
	vec3 diffuseCol=mat.baseColor.xyz;
	// Use material roughness or custom set roughness 
	float roughness= mat.roughness.x;
	float metallic = mat.metallic.x;

	vec3 viewDir=normalize(camPos-worldPos.xyz);

	vec3 lighting = vec3(0);
   // Direct Light loop
	for(int i=0;i<MAX_LIGHTS_PER_TILE;++i)
	{
		Light light=lights[i];
		vec3 lightPos=light.position.xyz;
		vec3 lightDir = normalize(lightPos - worldPos.xyz);
		vec3 halfwayDir = normalize(lightDir + viewDir);

		vec3 l = computePointLight(worldPos.xyz, lightPos, viewDir, normal, halfwayDir, light.color.xyz, diffuseCol, roughness, metallic);

		lighting.xyz += l;
	}
	outColor = vec4(lighting.xyz * 4, 1);
}
