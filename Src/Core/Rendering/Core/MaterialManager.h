#pragma once
#include "Defines/GlobalBuffers.h"

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

	Material* AllocateMaterial(const stltype::string& name, const Material& pMaterial);

	stltype::string_view GetMaterialName(const Material* pMat) const;

	void RebuildBufferData();

	const UBO::MaterialBuffer& GetMaterialBuffer() const
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
	UBO::MaterialBuffer m_materials;
	bool m_isBufferDirty{ true };
};
