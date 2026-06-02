#ifndef SHADERS_MATERIAL_HELPERS_H
#define SHADERS_MATERIAL_HELPERS_H

#include "Bindless.h"
#include "Material.h"
#include "Scene.h"
#include "UnrealPBR.h"

#ifndef __cplusplus
FUNC_QUALIFIER vec3 SampleMaterialBaseColorTexture(Material mat, vec2 materialUV)
{
    return IsMaterialFlagSet(mat.flags, MATERIAL_FLAG_DIFFUSE_BIT)
               ? texture(GlobalBindlessTextures[nonuniformEXT(mat.diffuseTexture)], materialUV).rgb
               : vec3(1.0);
}

FUNC_QUALIFIER vec4 SampleMaterialBaseColorTextureRGBA(Material mat, vec2 materialUV)
{
    return IsMaterialFlagSet(mat.flags, MATERIAL_FLAG_DIFFUSE_BIT)
               ? texture(GlobalBindlessTextures[nonuniformEXT(mat.diffuseTexture)], materialUV)
               : vec4(1.0);
}

FUNC_QUALIFIER vec3 SampleMaterialAlbedo(Material mat, vec2 materialUV)
{
    return mat.baseColor.rgb * SampleMaterialBaseColorTexture(mat, materialUV);
}

FUNC_QUALIFIER vec3 SampleMaterialEmissive(Material mat, vec2 materialUV)
{
    vec3 emissive = mat.emissive.rgb;
    if (IsMaterialFlagSet(mat.flags, MATERIAL_FLAG_EMISSIVE_BIT))
    {
        emissive *= texture(GlobalBindlessTextures[nonuniformEXT(mat.emissiveTexture)], materialUV).rgb;
    }
    return emissive;
}

FUNC_QUALIFIER vec3 ApplyMaterialNormalMap(Material mat, vec2 materialUV, mat3 tbn, vec3 fallbackNormal)
{
    if (!IsMaterialFlagSet(mat.flags, MATERIAL_FLAG_NORMAL_BIT))
        return fallbackNormal;

    vec4 normalSample = texture(GlobalBindlessTextures[nonuniformEXT(mat.normalTexture)], materialUV);
    vec3 tangentNormal;
    tangentNormal.xy = normalSample.xy * 2.0 - 1.0;

    if (IsMaterialFlagSet(mat.flags, MATERIAL_FLAG_FLIPPED_NORMAL_BIT))
    {
        tangentNormal.y = -tangentNormal.y;
    }

    tangentNormal.z = sqrt(clamp(1.0 - dot(tangentNormal.xy, tangentNormal.xy), 0.0, 1.0));

    vec3 mappedNormal = normalize(tbn * tangentNormal);
    return mappedNormal;
}

FUNC_QUALIFIER SurfaceParameters BuildMaterialSurface(Material mat, vec2 materialUV, vec3 materialAlbedo)
{
    SurfaceParameters surface = GetDefaultSurface(materialAlbedo, mat.pbr1.y, mat.pbr1.x);
    surface.subsurface = mat.pbr1.z;
    surface.specular = mat.pbr1.w;
    surface.anisotropic = mat.pbr2.x;
    surface.clearcoat = mat.pbr2.z;
    surface.clearcoatGloss = mat.pbr2.w;
    surface.sheen = mat.pbr3.x;

    if (IsMaterialFlagSet(mat.flags, MATERIAL_FLAG_METALLIC_ROUGHNESS_BIT))
    {
        vec3 sampledData = texture(GlobalBindlessTextures[nonuniformEXT(mat.metallicRoughnessTexture)], materialUV).rgb;
        surface.roughness *= sampledData.g;
        surface.metallic *= sampledData.b;
    }

    if (IsMaterialFlagSet(mat.flags, MATERIAL_FLAG_SPECULAR_GLOSSINESS_BIT))
    {
        vec4 specGloss = texture(GlobalBindlessTextures[nonuniformEXT(mat.specularTexture)], materialUV);
        // specGloss is a packed MR map: R=Metallic (from BGR swizzle), G=Roughness, B=AO
        surface.roughness = clamp(specGloss.g, 0.045, 1.0);
        surface.metallic = clamp(specGloss.r, 0.0, 1.0);
    }

    if (IsMaterialFlagSet(mat.flags, MATERIAL_FLAG_SHEEN_BIT))
    {
        surface.sheen *= texture(GlobalBindlessTextures[nonuniformEXT(mat.sheenTexture)], materialUV).r;
    }

    if (IsMaterialFlagSet(mat.flags, MATERIAL_FLAG_CLEARCOAT_BIT))
    {
        surface.clearcoat *= texture(GlobalBindlessTextures[nonuniformEXT(mat.clearcoatTexture)], materialUV).r;
    }

    return ApplyReflectanceDebug(surface, ubo.rtUseGlobalMaterialReflectance, ubo.rtGlobalMaterialReflectance);
}
#endif

#endif // SHADERS_MATERIAL_HELPERS_H
