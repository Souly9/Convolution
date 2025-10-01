#version 450 core
#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in IN
{
  vec4 worldPos;
  vec3 normal;
  vec2 fragTexCoord;
  uint matIdx;
};

layout(location = 0) out vec4 outColor;

void main() {
	outColor = vec4(1,1,0, 1);
}