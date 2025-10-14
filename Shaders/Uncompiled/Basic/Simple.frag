#version 450 core
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable
#define TransformSSBOSet 2
#define GBufferUBOSet 4
#define PassPerObjectDataSet 10
#include "../../Globals/GBufferUBO.h"
#include "../../Globals/Textures.h"
#include "../../Globals/PerObjectBuffers.h"

layout(location = 0) in VertexOut
{
  vec2 fragTexCoord;
} IN;

layout(location = 0) out vec4 outColor;

void main() {
	vec2 texCoords = vec2(1.0 - IN.fragTexCoord.x, 1.0 - IN.fragTexCoord.y);
	outColor = vec4(texture(GlobalBindlessTextures[gbufferUBO.gbufferAlbedoIdx], texCoords).xyz, 1);

	//vec4 texCoordMatData = vec4(texture(GlobalBindlessTextures[gbufferUBO.gbufferTexCoordMatIdx], texCoords));
	//uint fragMatIdx = uint(texCoordMatData.z);
	//Material fragMaterial = globalObjectDataSSBO.materials[fragMatIdx];
	//vec4 fragTexSample = vec4(texture(GlobalBindlessTextures[6], fragTexCoords));
	//vec4 texColor = vec4(fragTexSample.xyz, 1);
	vec4 uiColor = vec4(texture(GlobalBindlessTextures[gbufferUBO.gbufferUIIdx], texCoords));
	uiColor.a = mix(0, 0.8, uiColor.a);
	outColor = outColor * (1 - uiColor.a) + uiColor * uiColor.a;
}
