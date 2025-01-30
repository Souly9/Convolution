#pragma once
#include "RenderingTypeDefs.h"

struct MaterialProperties
{
	mathstl::Vector4 baseColor;
	mathstl::Vector4 metallic;
	mathstl::Vector4 emissive;
	mathstl::Vector4 roughness;
};

struct TextureData
{
	TextureHandle diffuseTexture;
	TextureHandle normalTexture;
};

struct Material
{
	MaterialProperties properties;
};

using MaterialBuffer = stltype::fixed_vector<Material, MAX_MATERIALS, false>;

class MaterialManager
{
public:

	MaterialManager();

	Material* GetMaterial(const stltype::string& name)
	{
		auto it = m_materialNameToBufferPos.find(name);

		if (it != m_materialNameToBufferPos.end())
		{
			return &m_materials.at(it->second);
		}

		return nullptr;
	}

	u32 GetMaterialIdx(Material* pMat)
	{
		return (u32)m_materialToBufferPosMap[pMat];
	}

	Material* AllocateMaterial(const stltype::string& name, const Material& pMaterial)
	{
		auto* pPtr = GetMaterial(name);
		if (pPtr)
			return pPtr;
		m_materialNameToBufferPos.emplace(name, m_materials.size());
		
		Material* pVecMaterial = &m_materials.emplace_back();
		pVecMaterial->properties = pMaterial.properties;
		//pVecMaterial->textures = pMaterial.textures;
		MarkMaterialsDirty();
		m_matToNameMap[pVecMaterial] = name;

		return pVecMaterial;
	}

	stltype::string_view GetMaterialName(const Material* pMat) const;

	void RebuildBufferData();

	const MaterialBuffer& GetMaterialBuffer() const
	{
		return m_materials;
	}

	void MarkMaterialsDirty()
	{
		m_isBufferDirty = true;
	}
	void MarkBufferUploaded()
	{
		m_isBufferDirty = false;
	}
	bool IsBufferDirty()
	{
		return m_isBufferDirty;
	}
private:
	stltype::hash_map<stltype::string, size_t> m_materialNameToBufferPos;
	// Material manager also manages the buffer hence we need to be able to return the index of a material in the buffer
	stltype::hash_map<Material*, u32> m_materialToBufferPosMap;
	stltype::hash_map<const Material*, stltype::string> m_matToNameMap;
	// Don't want to rebuild whole buffer when changing material values so we only upload them 
	//stltype::vector<u32> m_dirtyMaterials;
	MaterialBuffer m_materials;
	bool m_isBufferDirty{ true };
};

extern stltype::unique_ptr<MaterialManager> g_pMaterialManager;