#include "MtlDescriptorPool.h"
#include "Core/Rendering/Core/RenderingIncludes.h"

// TODO(Metal): argument buffer allocation and writes (Tier 2: write gpuResourceID/gpuAddress directly)

DescriptorSetMetal::DescriptorSetMetal()
{
}

DescriptorSetMetal::DescriptorSetMetal(u32 binding) : m_bindingSlot(binding)
{
}

void DescriptorSetMetal::SetBindingSlot(u32 binding)
{
    m_bindingSlot = binding;
}

MTL::Buffer* DescriptorSetMetal::GetRef() const
{
    return m_argumentBuffer;
}

void DescriptorSetMetal::WriteBufferUpdate(const GenBufferMetal& buffer, u32 bindingSlot)
{
}

void DescriptorSetMetal::WriteSSBOUpdate(const GenBufferMetal& buffer, u32 bindingSlot)
{
}

void DescriptorSetMetal::WriteBufferUpdate(
    const GenBufferMetal& buffer, bool isUBO, u32 size, u32 bindingSlot, u32 offset)
{
}

void DescriptorSetMetal::WriteAccelerationStructureUpdate(const AccelerationStructure& accelerationStructure,
                                                          u32 bindingSlot)
{
}

void DescriptorSetMetal::WriteBindlessImageUpdate(const TextureMetal* pTex, u32 idx, u32 bindingSlot)
{
}

DescriptorPoolMetal::DescriptorPoolMetal()
{
}

DescriptorPoolMetal::~DescriptorPoolMetal()
{
}

void DescriptorPoolMetal::Create(const DescriptorPoolCreateInfo& createInfo)
{
}

DescriptorSetMetal* DescriptorPoolMetal::CreateDescriptorSet(const DescriptorSetLayoutMetal& layout)
{
    return nullptr;
}
