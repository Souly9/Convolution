#pragma once
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Vulkan/VkBackendAccess.h"
#include "Core/Rendering/Vulkan/VkDescriptorSetLayout.h"
#include "Core/Rendering/Vulkan/Utils/VkEnumHelpers.h"

static inline VkDescriptorType Conv(const DescriptorType& m)
{
    switch (m)
    {
        case DescriptorType::UniformBuffer:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case DescriptorType::StorageBuffer:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case DescriptorType::AccelerationStructure:
            return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        case DescriptorType::CombinedImageSampler:
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case DescriptorType::BindlessImages:
            return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case DescriptorType::BindlessSampledImages:
            return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case DescriptorType::Samplers:
            return VK_DESCRIPTOR_TYPE_SAMPLER;
        default:
            DEBUG_ASSERT(false);
    }

    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

namespace DescriptorLayoutUtils
{
static inline DescriptorSetLayout CreateOneDescriptorSetForAll(
    const stltype::vector<PipelineDescriptorLayout>& layoutInfo)
{
    stltype::vector<VkDescriptorSetLayoutBinding> bindings;
    stltype::vector<VkDescriptorBindingFlags> flags;

    bindings.reserve(layoutInfo.size());
    flags.reserve(layoutInfo.size());

    bool needsToSupportBindless = false;
    for (const auto& layout : layoutInfo)
    {
        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.binding = layout.bindingSlot;
        layoutBinding.descriptorType = Conv(layout.type);
        layoutBinding.descriptorCount = layout.descriptorCount;
        layoutBinding.stageFlags = Conv(layout.shaderStagesToBind);
        layoutBinding.pImmutableSamplers = nullptr;

        bindings.push_back(layoutBinding);
        flags.push_back(layout.IsBindless()
                            ? VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
                            : 0);

        needsToSupportBindless = needsToSupportBindless || layout.IsBindless();
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{};
    bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    bindingFlags.pNext = nullptr;
    bindingFlags.bindingCount = flags.size();
    bindingFlags.pBindingFlags = flags.data();

    VkDescriptorSetLayoutCreateInfo descriptorLayout{};
    descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayout.bindingCount = bindings.size();
    descriptorLayout.pBindings = bindings.data();
    descriptorLayout.pNext = &bindingFlags;

    if (needsToSupportBindless)
    {
        descriptorLayout.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    }

    VkDescriptorSetLayout setLayout;
    DEBUG_ASSERT(vkCreateDescriptorSetLayout(VkBackend::Device(), &descriptorLayout, VulkanAllocator(), &setLayout) ==
                 VK_SUCCESS);
    return {setLayout};
}

static inline DescriptorSetLayout CreateOneDescriptorSetLayout(const PipelineDescriptorLayout& layout)
{
    stltype::vector<PipelineDescriptorLayout> rslt{layout};
    return CreateOneDescriptorSetForAll(rslt);
}

static inline stltype::vector<DescriptorSetLayout> CreateOneDescriptorSetLayoutPerSet(
    const stltype::vector<PipelineDescriptorLayout>& layoutInfos)
{
    stltype::vector<DescriptorSetLayout> rslt;

    stltype::hash_map<u32, stltype::vector<PipelineDescriptorLayout>> setLayoutMap;
    u32 setCount = 0;
    for (const auto& layout : layoutInfos)
    {
        if (setCount < layout.setIndex)
            setCount = layout.setIndex;
        setLayoutMap[layout.setIndex].push_back(layout);
    }
    for (u32 i = 0; i <= setCount; ++i)
    {
        rslt.push_back(CreateOneDescriptorSetForAll(setLayoutMap[i]));
    }
    return rslt;
}
} // namespace DescriptorLayoutUtils
