#ifndef SHADERS_MATERIAL_H
#define SHADERS_MATERIAL_H

#include "Types.h"

// Material Flags Bit Positions
#define MATERIAL_FLAG_DIFFUSE_BIT            0
#define MATERIAL_FLAG_FLIPPED_NORMAL_BIT     1
#define MATERIAL_FLAG_NORMAL_BIT             2
#define MATERIAL_FLAG_METALLIC_ROUGHNESS_BIT 3
#define MATERIAL_FLAG_EMISSIVE_BIT           4
#define MATERIAL_FLAG_SHEEN_BIT              5
#define MATERIAL_FLAG_CLEARCOAT_BIT          6

struct Material
{
    vec4 baseColor;   
    vec4 metallic;    
    vec4 emissive;    
    vec4 roughness;  
    vec4 anisotropicTintIorClearcoat;
    vec4 glossSheenSTransFlatness;   
    uint diffuseTexture;           
    uint normalTexture;         
    uint metallicRoughnessTexture;
    uint emissiveTexture;
    uint sheenTexture;
    uint clearcoatTexture;
    uint flags;
    uint _pad;
}; 

FUNC_QUALIFIER bool IsMaterialFlagSet(uint flags, uint bit) 
{ 
    return (flags & (1u << bit)) != 0u; 
}

FUNC_QUALIFIER void SetMaterialFlag(INOUT(uint) flags, uint bit, bool value) 
{ 
    if (value) 
        flags |= (1u << bit); 
    else 
        flags &= ~(1u << bit); 
}

#endif // SHADERS_MATERIAL_H