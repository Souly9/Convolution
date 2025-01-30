#include "Core/Global/GlobalDefines.h"
#include "Material.h"

MaterialManager::MaterialManager()
{
	m_materials.reserve(MAX_MATERIALS);
	Material defaultMaterial;
	defaultMaterial.properties.baseColor = mathstl::Vector4(1.0f, 0.0f, 1.0f, 1.0f);

	AllocateMaterial("Default", defaultMaterial);

	g_pEventSystem->AddNextFrameEventCallback([this](const auto&)
		{
			if(IsBufferDirty())
				RebuildBufferData();
			else
			{
				const auto nullMatCount = stltype::count_if(m_materials.begin(), m_materials.end(),
					[](const Material& mat) { return &mat == nullptr; }
				);
				if (nullMatCount > 10)
					RebuildBufferData();
			}
		}
	);
}

stltype::string_view MaterialManager::GetMaterialName(const Material* pMat) const
{
	if (auto it = m_matToNameMap.find(pMat); it != m_matToNameMap.cend())
	{
		return it->second;
	}
	return stltype::string_view();
}

void MaterialManager::RebuildBufferData()
{
	m_materialNameToBufferPos.clear();
	m_materialToBufferPosMap.clear();

	m_materials.erase(
		stltype::find_if(m_materials.begin(), m_materials.end(),
			[](const Material& mat) { return &mat == nullptr; }),
		m_materials.end());

	for (u32 i = 0; i < m_materials.size(); ++i)
	{
		Material* pMat = &m_materials.at(i);
		m_materialToBufferPosMap[pMat] = i;
		m_materialNameToBufferPos[m_matToNameMap[pMat]] = i;
	}
}