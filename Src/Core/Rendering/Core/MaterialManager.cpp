#include "MaterialManager.h"
#include "Core/Global/GlobalVariables.h"

MaterialManager::MaterialManager()
{
    m_materials.reserve(MAX_MATERIALS);
    Material defaultMaterial{};
    defaultMaterial.baseColor = mathstl::Vector4(1.0f, 0.0f, 1.0f, 1.0f);
    defaultMaterial.emissive = mathstl::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    defaultMaterial.pbr1 = mathstl::Vector4(0.0f, 1.0f, 0.0f, 0.5f);
    defaultMaterial.pbr2 = mathstl::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
    defaultMaterial.pbr3 = mathstl::Vector4(0.0f, 0.5f, 0.0f, 1.5f);
    defaultMaterial.diffuseTexture = 0;
    defaultMaterial.specularTexture = 0;
    defaultMaterial.flags = 0;

    AllocateMaterial("Default", defaultMaterial);
}

static bool CompareMaterials(const Material& m1, const Material& m2)
{
    return m1.baseColor == m2.baseColor && m1.emissive == m2.emissive && m1.pbr1 == m2.pbr1 && m1.pbr2 == m2.pbr2 &&
           m1.pbr3 == m2.pbr3 && m1.diffuseTexture == m2.diffuseTexture && m1.normalTexture == m2.normalTexture &&
           m1.metallicRoughnessTexture == m2.metallicRoughnessTexture && m1.emissiveTexture == m2.emissiveTexture &&
           m1.sheenTexture == m2.sheenTexture && m1.clearcoatTexture == m2.clearcoatTexture &&
           m1.specularTexture == m2.specularTexture && m1.flags == m2.flags;
}

Material* MaterialManager::GetMaterial(const stltype::string& name)
{
    SimpleScopedGuard<CustomMutex> lock(m_mutex);
    auto it = m_materialNameToBufferPos.find(name);
    if (it != m_materialNameToBufferPos.end())
    {
        return &m_materials.at(it->second);
    }
    return nullptr;
}

Material* MaterialManager::AllocateMaterial(const stltype::string& name, const Material& pMaterial)
{
    SimpleScopedGuard<CustomMutex> lock(m_mutex);

    stltype::string uniqueName = name;
    u32 collisionIndex = 1;

    while (true)
    {
        auto itExisting = m_materialNameToBufferPos.find(uniqueName);
        if (itExisting == m_materialNameToBufferPos.end())
        {
            break;
        }

        if (CompareMaterials(m_materials.at(itExisting->second), pMaterial))
        {
            return &m_materials.at(itExisting->second);
        }

        uniqueName = name + "_" + stltype::to_string(collisionIndex++);
    }

    if (m_materials.size() >= MAX_MATERIALS)
    {
        DEBUG_ASSERT(false);
        return nullptr;
    }

    m_materials.push_back(pMaterial);
    u32 materialIdx = (u32)m_materials.size() - 1;
    Material* pVecMaterial = &m_materials.back();
    m_numAllocatedMaterials = (u32)m_materials.size();

    m_materialToBufferPosMap[pVecMaterial] = materialIdx;
    m_materialNameToBufferPos.emplace(uniqueName, materialIdx);

    MarkMaterialsDirty();
    m_matToNameMap[pVecMaterial] = uniqueName;

    return pVecMaterial;
}

u32 MaterialManager::GetMaterialIdx(Material* pMat)
{
    SimpleScopedGuard<CustomMutex> lock(m_mutex);

    if (pMat == nullptr)
        return 0;
    if (m_isBufferDirty.load(stltype::memory_order_relaxed))
        RebuildBufferDataUnlocked();
    if (auto it = m_materialToBufferPosMap.find(pMat); it != m_materialToBufferPosMap.end())
    {
        return it->second;
    }
    // Shouldn't happen as we should always allocate all materials
    DEBUG_ASSERT(false);
    return 0;
}

stltype::string_view MaterialManager::GetMaterialName(const Material* pMat) const
{
    SimpleScopedGuard<CustomMutex> lock(m_mutex);
    if (auto it = m_matToNameMap.find(pMat); it != m_matToNameMap.cend())
    {
        return it->second;
    }
    return stltype::string_view();
}

void MaterialManager::RebuildBufferData()
{
    SimpleScopedGuard<CustomMutex> lock(m_mutex);
    RebuildBufferDataUnlocked();
}

void MaterialManager::RebuildBufferDataUnlocked()
{
    const auto oldNameToPos = m_materialNameToBufferPos;
    m_materialNameToBufferPos.clear();
    m_materialToBufferPosMap.clear();
    m_matToNameMap.clear();

    for (u32 i = 0; i < m_numAllocatedMaterials; ++i)
    {
        Material* pMat = &m_materials.at(i);
        m_materialToBufferPosMap[pMat] = i;
    }

    for (const auto& [name, idx] : oldNameToPos)
    {
        if (idx < m_numAllocatedMaterials)
        {
            const Material* pMat = &m_materials.at((u32)idx);
            m_materialNameToBufferPos[name] = (size_t)idx;
            m_matToNameMap[pMat] = name;
        }
    }
}
