#pragma once
#include "MtlBackendDefines.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/DescriptorPool.h"


class GenBufferMetal;
class TextureMetal;
class DescriptorSetLayoutMetal;

// A descriptor set is a region of an argument buffer; resources written here must also be made
// resident (useResource/useHeap or MTL::ResidencySet) before the encoder that reads them
class DescriptorSetMetal : public DescriptorSetBase
{
public:
    DescriptorSetMetal();
    DescriptorSetMetal(u32 binding);

    void SetBindingSlot(u32 binding);

    MTL::Buffer* GetRef() const;

    void WriteBufferUpdate(const GenBufferMetal& buffer, u32 bindingSlot = 0);
    void WriteSSBOUpdate(const GenBufferMetal& buffer, u32 bindingSlot = 0);
    void WriteBufferUpdate(const GenBufferMetal& buffer, bool isUBO, u32 size, u32 bindingSlot = 0, u32 offset = 0);
    void WriteAccelerationStructureUpdate(const AccelerationStructure& accelerationStructure, u32 bindingSlot = 0);
    void WriteBindlessImageUpdate(const TextureMetal* pTex, u32 idx, u32 bindingSlot = 0);

private:
    MTL::Buffer* m_argumentBuffer{nullptr};
    u64 m_offset{0};
    u32 m_bindingSlot{0};
};


// Sub-allocates descriptor sets from one large argument buffer
class DescriptorPoolMetal : public DescriptorPoolBase
{
public:
    DescriptorPoolMetal();
    ~DescriptorPoolMetal();

    DescriptorPoolMetal(const DescriptorPoolMetal&) = delete;
    DescriptorPoolMetal& operator=(const DescriptorPoolMetal&) = delete;
    DescriptorPoolMetal(DescriptorPoolMetal&&) = default;
    DescriptorPoolMetal& operator=(DescriptorPoolMetal&&) = default;

    void Create(const DescriptorPoolCreateInfo& createInfo);

    DescriptorSetMetal* CreateDescriptorSet(const DescriptorSetLayoutMetal& layout);

    bool IsValid() const
    {
        return m_argumentBuffer != nullptr;
    }

protected:
    stltype::vector<stltype::unique_ptr<DescriptorSetMetal>> m_createdDescriptorSets{};
    MTL::Buffer* m_argumentBuffer{nullptr};
    u32 m_descriptorSetCount = 0;
};
