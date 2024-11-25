#pragma once
#include "RenderingTypeDefs.h"

struct MaterialProperties
{
	DirectX::XMFLOAT3 baseColor;
	DirectX::XMFLOAT3 metallic;
	DirectX::XMFLOAT3 emissive;
	DirectX::XMFLOAT3 roughness;
};

struct TextureData
{
	TextureHandle diffuseTexture;
	TextureHandle normalTexture;
};

struct Material
{
	MaterialProperties properties;
	TextureData textures;
};

class MaterialManager
{
public:

	MaterialManager()
	{
		m_materials.reserve(MAX_MATERIALS);
	}

	Material* GetMaterial(const stltype::string& name)
	{
		auto it = m_materials.find(name);

		if (it != m_materials.end())
		{
			return &it->second;
		}

		return nullptr;
	}

	Material* AllocateMaterial(const stltype::string& name, const Material& pMaterial)
	{
		auto* pPtr = GetMaterial(name);
		if (pPtr)
			return pPtr;
		m_materials.insert({ name, Material{} });
		Material* pVecMaterial = &m_materials[name];
		pVecMaterial->properties = pMaterial.properties;
		pVecMaterial->textures = pMaterial.textures;

		return pVecMaterial;
	}


private:
	stltype::hash_map<stltype::string, Material> m_materials;
};

extern stltype::unique_ptr<MaterialManager> g_pMaterialManager;