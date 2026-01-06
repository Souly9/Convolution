#include "MaterialManager.h"
#include "Core/Events/EventSystem.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"

MaterialManager::MaterialManager()
{
    m_materials.reserve(MAX_MATERIALS);
    Material defaultMaterial;
    defaultMaterial.properties.baseColor = mathstl::Vector4(1.0f, 0.0f, 1.0f, 1.0f);
    defaultMaterial.diffuseTexture = 0;

    AllocateMaterial("Default", defaultMaterial);

    g_pEventSystem->AddNextFrameEventCallback(
        [this](const auto&)
        {
            if (IsBufferDirty())
                RebuildBufferData();
            else
            {
                const auto nullMatCount = stltype::count_if(
                    m_materials.begin(), m_materials.end(), [](const Material& mat) { return &mat == nullptr; });
                if (nullMatCount > 10)
                    RebuildBufferData();
            }
        });
}

Material* MaterialManager::AllocateMaterial(const stltype::string& name, const Material& pMaterial)
{
    auto* pPtr = GetMaterial(name);
    if (pPtr)
        return pPtr;
    m_materialNameToBufferPos.emplace(name, m_materials.size());

    Material* pVecMaterial = &m_materials.emplace_back();
    pVecMaterial->properties = pMaterial.properties;
    pVecMaterial->diffuseTexture = pMaterial.diffuseTexture;
    pVecMaterial->normalTexture = pMaterial.normalTexture;
    MarkMaterialsDirty();
    m_matToNameMap[pVecMaterial] = name;

    return pVecMaterial;
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
        stltype::find_if(m_materials.begin(), m_materials.end(), [](const Material& mat) { return &mat == nullptr; }),
        m_materials.end());

    for (u32 i = 0; i < m_materials.size(); ++i)
    {
        Material* pMat = &m_materials.at(i);
        m_materialToBufferPosMap[pMat] = i;
        m_materialNameToBufferPos[m_matToNameMap[pMat]] = i;
    }
}