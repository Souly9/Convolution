#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in VertexOut
{
  vec4 worldPos;
  vec3 normal;
  vec2 fragTexCoord;
  uint matIdx;
} IN;

layout(location = 0) out vec4 outColor;

void main() {

	outColor = vec4(1,1,0, 1);
}