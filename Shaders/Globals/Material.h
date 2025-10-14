
struct Material
{
    vec4 baseColor;   // Offset: 0
    vec4 metallic;    // Offset: 16
    vec4 emissive;    // Offset: 32
    vec4 roughness;   // Offset: 48
    uint  diffuseTexture;           
    uint  normalTexture;         
    uint tex3;
    uint tex4;          
}; 