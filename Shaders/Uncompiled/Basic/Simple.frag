#version 450 core
#extension GL_ARB_shading_language_include : enable
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
	vec2 texCoords = vec2(1.0 - IN.fragTexCoord.x, 1.0 - IN.fragTexCoord.y);
	outColor = vec4(texture(GlobalBindlessTextures[gbufferUBO.gbufferPositionIdx], texCoords).xyz, 1);
	vec4 uiColor = vec4(texture(GlobalBindlessTextures[gbufferUBO.gbufferUIIdx], texCoords));
	uiColor.a = mix(0, 0.8, uiColor.a);
	outColor = outColor * (1 - uiColor.a) + uiColor * uiColor.a;
}
