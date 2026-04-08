#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Defines/GlobalBuffers.h"
#include "Core/Global/ThreadBase.h"

class MaterialManager
{
public:
    MaterialManager();

    Material* GetMaterial(const stltype::string& name);

    u32 GetMaterialIdx(Material* pMat);

    Material* AllocateMaterial(const stltype::string& name, const Material& pMaterial);

    stltype::string_view GetMaterialName(const Material* pMat) const;

    void RebuildBufferData();

    const UBO::MaterialBuffer& GetMaterialBuffer() const
    {
        return m_materials;
    }

    void MarkMaterialsDirty()
    {
        m_isBufferDirty.store(true, stltype::memory_order_relaxed);
    }
    void MarkBufferUploaded()
    {
        m_isBufferDirty.store(false, stltype::memory_order_relaxed);
    }
    bool IsBufferDirty()
    {
        return m_isBufferDirty.load(stltype::memory_order_relaxed);
    }

private:
    void RebuildBufferDataUnlocked();

private:
    stltype::hash_map<stltype::string, size_t> m_materialNameToBufferPos{};
    // Material manager also manages the buffer hence we need to be able to return the index of a material in the buffer
    stltype::hash_map<Material*, u32> m_materialToBufferPosMap{};
    stltype::hash_map<const Material*, stltype::string> m_matToNameMap{};
    // Don't want to rebuild whole buffer when changing material values so we only upload them
    // stltype::vector<u32> m_dirtyMaterials;
    UBO::MaterialBuffer m_materials;
    u32 m_numAllocatedMaterials{0};
    stltype::atomic<bool> m_isBufferDirty{true};
    mutable CustomMutex m_mutex;
};
