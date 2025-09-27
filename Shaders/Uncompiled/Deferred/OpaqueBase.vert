#version 450
#define ViewUBOSet 1
#define TransformSSBOSet 2
#define PassPerObjectDataSet 3
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord0;

layout(location = 0) out VertexOut
{
  vec4 worldPos;
  vec2 fragTexCoord;
} OUT;

void main() { 
   // OUT.fragTexCoord = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    //OUT.worldPos = vec4(OUT.fragTexCoord * 2.0f + -1.0f, 0.0f, 1.0f);
    //OUT.fragTexCoord = (inPosition.xy + 1) * 0.5;
    OUT.worldPos = vec4(1,1,1, 1.0);
}
