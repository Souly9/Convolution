#ifndef SHADERS_MATERIAL_H
#define SHADERS_MATERIAL_H
#include "Types.h"

#define MATERIAL_FLAG_DIFFUSE_BIT            0
#define MATERIAL_FLAG_FLIPPED_NORMAL_BIT     1
#define MATERIAL_FLAG_NORMAL_BIT             2
#define MATERIAL_FLAG_METALLIC_ROUGHNESS_BIT 3
#define MATERIAL_FLAG_EMISSIVE_BIT           4
#define MATERIAL_FLAG_SHEEN_BIT              5
#define MATERIAL_FLAG_CLEARCOAT_BIT          6
#define MATERIAL_FLAG_SPECULAR_GLOSSINESS_BIT 7

STRUCTDECL(Material)
    STRUCTFIELD(vec4, baseColor) // RGBA
    STRUCTFIELD(vec4, emissive)  // RGB (A unused for now)
    STRUCTFIELD(vec4, pbr1)      // x: metallic, y: roughness, z: subsurface, w: specular
    STRUCTFIELD(vec4, pbr2)      // x: anisotropic, y: specularTint, z: clearcoat, w: clearcoatGloss
    STRUCTFIELD(vec4, pbr3)      // x: sheen, y: sheenTint, z: specTrans, w: ior
    STRUCTFIELD(uint, diffuseTexture)
    STRUCTFIELD(uint, normalTexture)
    STRUCTFIELD(uint, metallicRoughnessTexture)
    STRUCTFIELD(uint, emissiveTexture)
    STRUCTFIELD(uint, sheenTexture)
    STRUCTFIELD(uint, clearcoatTexture)
    STRUCTFIELD(uint, specularTexture)
    STRUCTFIELD(uint, flags)
STRUCTEND()

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
