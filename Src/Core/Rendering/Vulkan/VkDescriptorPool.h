#pragma once
#include "BackendDefines.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/DescriptorPool.h"

static inline constexpr u32 MAX_DESCRIPTOR_SETS = 8192;

class DescriptorSetVulkan : public DescriptorSetBase
{
public:
    DescriptorSetVulkan();
    DescriptorSetVulkan(u32 binding);
    DescriptorSetVulkan(VkDescriptorSet descriptorSet) : m_descriptorSet(descriptorSet)
    {
    }

    void SetBindingSlot(u32 binding);

    VkDescriptorSet GetRef() const;

    // Updates the descriptor set's buffer with the supplied data
    void WriteBufferUpdate(const GenBufferVulkan& buffer, u32 bindingSlot = 0);
    void WriteSSBOUpdate(const GenBufferVulkan& buffer, u32 bindingSlot = 0);
    void WriteBufferUpdate(const GenBufferVulkan& buffer, bool isUBO, u32 size, u32 bindingSlot = 0, u32 offset = 0);
    void WriteAccelerationStructureUpdate(const AccelerationStructure& accelerationStructure, u32 bindingSlot = 0);
    void WriteBindlessImageUpdate(const TextureVulkan* pTex, u32 idx, u32 bindingSlot = 0);
    void WriteBindlessSampledImageUpdate(const TextureVulkan* pTex, u32 idx, u32 bindingSlot);
    void WriteSamplerUpdate(VkSampler sampler, u32 idx, u32 bindingSlot);

    virtual void NamingCallBack(const stltype::string& name) override;

private:
    VkDescriptorSet m_descriptorSet{VK_NULL_HANDLE};
    u32 m_bindingSlot{0};
};

struct DescriptorPoolCreateInfo
{
    u32 maxSets{8192};
    bool enableBindlessTextureDescriptors{true};
    bool enableStorageBufferDescriptors{false};
    bool enableAccelerationStructureDescriptors{false};
    bool freeDescriptorSet{true};
};

class DescriptorPoolVulkan : public DescriptorPoolBase
{
public:
    DescriptorPoolVulkan();
    ~DescriptorPoolVulkan();

    DescriptorPoolVulkan(const DescriptorPoolVulkan&) = delete;
    DescriptorPoolVulkan& operator=(const DescriptorPoolVulkan&) = delete;
    DescriptorPoolVulkan(DescriptorPoolVulkan&&) = default;
    DescriptorPoolVulkan& operator=(DescriptorPoolVulkan&&) = default;

    void Create(const DescriptorPoolCreateInfo& createInfo);

    stltype::vector<DescriptorSetVulkan*> CreateDescriptorSetsUBO(
        const stltype::vector<VkDescriptorSetLayout>& layouts);
    DescriptorSetVulkan* CreateDescriptorSet(const VkDescriptorSetLayout& layouts);
    DescriptorSetVulkan* CreateDescriptorSet(const DescriptorSetLayout& layouts);

    bool IsValid() const
    {
        return m_descriptorPool != VK_NULL_HANDLE;
    }

    VkDescriptorPool GetRef() const
    {
        return m_descriptorPool;
    }

    virtual void NamingCallBack(const stltype::string& name) override;

protected:
    VkDescriptorPoolSize CreateNewPoolSizeForType(VkDescriptorType type, u32 count) const;

protected:
    stltype::vector<stltype::unique_ptr<DescriptorSetVulkan>> m_createdDescriptorSets{};
    VkDescriptorPool m_descriptorPool{VK_NULL_HANDLE};
    u32 m_descriptorSetCount = 0;
};
