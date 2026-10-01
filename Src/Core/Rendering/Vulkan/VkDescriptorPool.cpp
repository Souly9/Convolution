#include "VkDescriptorPool.h"
#include "Core/Rendering/Core/BindlessTexturesDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/DescriptorSetLayout.h"
#include "Core/Global/GlobalDefines.h"
#include "VkAccelerationStructure.h"
#include "VkBackendAccess.h"
#include "VkTexture.h"

DescriptorPoolVulkan::DescriptorPoolVulkan()
{
}

DescriptorPoolVulkan::~DescriptorPoolVulkan()
{
    vkDestroyDescriptorPool(VkBackend::Device(), m_descriptorPool, VulkanAllocator());
}

void DescriptorPoolVulkan::Create(const DescriptorPoolCreateInfo& createInfo)
{
    const u32 maxSets = createInfo.maxSets > 0 ? createInfo.maxSets : MAX_DESCRIPTOR_SETS;
    stltype::vector<VkDescriptorPoolSize> poolSizes;

    poolSizes.push_back(CreateNewPoolSizeForType(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxSets));

    if (createInfo.enableBindlessTextureDescriptors)
    {
        const u32 bindlessTextureCount = g_renderer.GetBindlessCapacity(Bindless::BindlessType::GlobalTextures);
        const u32 storageImageCount = stltype::max(maxSets * 2, bindlessTextureCount * 4);
        // Two bindless sets, each with a 2D and a 2D array texture array
        const u32 sampledImageCount =
            2 * (bindlessTextureCount + g_renderer.GetBindlessCapacity(Bindless::BindlessType::GlobalArrayTextures));
        // Only ImGui still uses combined image samplers
        poolSizes.push_back(CreateNewPoolSizeForType(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxSets * 4));
        poolSizes.push_back(CreateNewPoolSizeForType(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, storageImageCount));
        poolSizes.push_back(CreateNewPoolSizeForType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, sampledImageCount));
        poolSizes.push_back(CreateNewPoolSizeForType(VK_DESCRIPTOR_TYPE_SAMPLER, maxSets));
    }
    if (createInfo.enableStorageBufferDescriptors)
    {
        poolSizes.push_back(CreateNewPoolSizeForType(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, maxSets));
    }
    if (createInfo.enableAccelerationStructureDescriptors)
    {
        poolSizes.push_back(
            CreateNewPoolSizeForType(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, maxSets));
    }

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;

    poolInfo.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;

    if (createInfo.freeDescriptorSet)
        poolInfo.flags |= VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    poolInfo.poolSizeCount = poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = maxSets;

    DEBUG_ASSERT(vkCreateDescriptorPool(VkBackend::Device(), &poolInfo, VulkanAllocator(), &m_descriptorPool) ==
                 VK_SUCCESS);
}

stltype::vector<DescriptorSetVulkan*> DescriptorPoolVulkan::CreateDescriptorSetsUBO(
    const stltype::vector<VkDescriptorSetLayout>& layouts)
{
    if (m_descriptorSetCount + layouts.size() > MAX_DESCRIPTOR_SETS)
    {
        DEBUG_LOG_ERRF("DescriptorPoolVulkan: Exceeded MAX_DESCRIPTOR_SETS limit ({} + {} > {})",
                       m_descriptorSetCount, layouts.size(), MAX_DESCRIPTOR_SETS);
        return {};
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = static_cast<u32>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();

    stltype::vector<VkDescriptorSet> descriptorSets;
    descriptorSets.resize(layouts.size());
    const VkResult allocRes = vkAllocateDescriptorSets(VkBackend::Device(), &allocInfo, descriptorSets.data());
    if (allocRes != VK_SUCCESS)
    {
        DEBUG_LOG_ERRF("DescriptorPoolVulkan: vkAllocateDescriptorSets failed with error: {}", static_cast<int>(allocRes));
        return {};
    }

    stltype::vector<DescriptorSetVulkan*> rslt;
    rslt.reserve(layouts.size());
    for (u32 i = 0; i < descriptorSets.size(); ++i)
    {
        auto pSet = stltype::make_unique<DescriptorSetVulkan>(descriptorSets[i]);
        rslt.push_back(pSet.get());
        m_createdDescriptorSets.push_back(stltype::move(pSet));
    }
    m_descriptorSetCount += static_cast<u32>(layouts.size());

    return rslt;
}

DescriptorSetVulkan* DescriptorPoolVulkan::CreateDescriptorSet(const VkDescriptorSetLayout& layout)
{
    if (m_descriptorSetCount + 1 > MAX_DESCRIPTOR_SETS)
    {
        DEBUG_LOG_ERRF("DescriptorPoolVulkan: Exceeded MAX_DESCRIPTOR_SETS limit ({} >= {})",
                       m_descriptorSetCount, MAX_DESCRIPTOR_SETS);
        return nullptr;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    const VkResult allocRes = vkAllocateDescriptorSets(VkBackend::Device(), &allocInfo, &descriptorSet);
    if (allocRes != VK_SUCCESS || descriptorSet == VK_NULL_HANDLE)
    {
        DEBUG_LOG_ERRF("DescriptorPoolVulkan: vkAllocateDescriptorSets failed with error: {}", static_cast<int>(allocRes));
        return nullptr;
    }

    auto pSet = stltype::make_unique<DescriptorSetVulkan>(descriptorSet);
    auto* pRet = pSet.get();
    m_createdDescriptorSets.push_back(stltype::move(pSet));

    ++m_descriptorSetCount;

    return pRet;
}

DescriptorSetVulkan* DescriptorPoolVulkan::CreateDescriptorSet(const DescriptorSetLayout& layout)
{
    return CreateDescriptorSet(layout.GetRef());
}

VkDescriptorPoolSize DescriptorPoolVulkan::CreateNewPoolSizeForType(VkDescriptorType type, u32 count) const
{
    VkDescriptorPoolSize poolSize{};
    poolSize.type = type;
    poolSize.descriptorCount = count;
    return poolSize;
}

DescriptorSetVulkan::DescriptorSetVulkan()
{
}

void DescriptorSetVulkan::SetBindingSlot(u32 binding)
{
    m_bindingSlot = binding;
}

VkDescriptorSet DescriptorSetVulkan::GetRef() const
{
    return m_descriptorSet;
}

void DescriptorSetVulkan::WriteBufferUpdate(const GenBufferVulkan& buffer, u32 bindingSlot)
{
    WriteBufferUpdate(buffer, true, buffer.GetInfo().size, bindingSlot, 0);
}

void DescriptorSetVulkan::WriteSSBOUpdate(const GenBufferVulkan& buffer, u32 bindingSlot)
{
    WriteBufferUpdate(buffer, false, buffer.GetInfo().size, bindingSlot, 0);
}

void DescriptorSetVulkan::WriteBufferUpdate(
    const GenBufferVulkan& buffer, bool isUBO, u32 size, u32 bindingSlot, u32 offset)
{
    if (GetRef() == VK_NULL_HANDLE)
    {
        DEBUG_LOG_ERR("DescriptorSetVulkan: Attempted WriteBufferUpdate on VK_NULL_HANDLE descriptor set");
        return;
    }

    if (bindingSlot == 0)
        bindingSlot = m_bindingSlot;
    DEBUG_ASSERT(bindingSlot != 0);

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer.GetRef();
    bufferInfo.offset = offset;
    bufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = GetRef();
    descriptorWrite.dstBinding = bindingSlot;
    descriptorWrite.dstArrayElement = offset;
    descriptorWrite.descriptorType = isUBO ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;
    descriptorWrite.pImageInfo = nullptr;       // Optional
    descriptorWrite.pTexelBufferView = nullptr; // Optional

    vkUpdateDescriptorSets(VkBackend::Device(), 1, &descriptorWrite, 0, nullptr);
}

void DescriptorSetVulkan::WriteAccelerationStructureUpdate(const AccelerationStructure& accelerationStructure,
                                                           u32 bindingSlot)
{
    if (GetRef() == VK_NULL_HANDLE)
    {
        DEBUG_LOG_ERR("DescriptorSetVulkan: Attempted WriteAccelerationStructureUpdate on VK_NULL_HANDLE descriptor set");
        return;
    }

    const auto& vkAccelerationStructure = static_cast<const AccelerationStructureVulkan&>(accelerationStructure);

    VkAccelerationStructureKHR nativeHandle =
        reinterpret_cast<VkAccelerationStructureKHR>(vkAccelerationStructure.GetNativeHandle());
    VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureInfo{};
    accelerationStructureInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    accelerationStructureInfo.accelerationStructureCount = 1;
    accelerationStructureInfo.pAccelerationStructures = &nativeHandle;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.pNext = &accelerationStructureInfo;
    descriptorWrite.dstSet = GetRef();
    descriptorWrite.dstBinding = bindingSlot == 0 ? m_bindingSlot : bindingSlot;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    descriptorWrite.descriptorCount = 1;

    vkUpdateDescriptorSets(VkBackend::Device(), 1, &descriptorWrite, 0, nullptr);
}

static VkImageLayout GetSampledImageLayout(const TextureVulkan* pTex)
{
    const auto usage = (u32)pTex->GetInfo().usage;
    const bool isDepthStencil = (usage & (u32)Usage::DepthAttachment) || (usage & (u32)Usage::StencilAttachment) ||
                                (usage & (u32)Usage::ShadowMap);
    return isDepthStencil ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void DescriptorSetVulkan::WriteBindlessSampledImageUpdate(const TextureVulkan* pTex, u32 idx, u32 bindingSlot)
{
    if (GetRef() == VK_NULL_HANDLE)
    {
        DEBUG_LOG_ERR("DescriptorSetVulkan: Attempted WriteBindlessSampledImageUpdate on VK_NULL_HANDLE descriptor set");
        return;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = GetSampledImageLayout(pTex);
    imageInfo.imageView = pTex->GetImageView();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = GetRef();
    descriptorWrite.dstBinding = bindingSlot;
    descriptorWrite.dstArrayElement = idx;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(VkBackend::Device(), 1, &descriptorWrite, 0, nullptr);
}

void DescriptorSetVulkan::WriteSamplerUpdate(VkSampler sampler, u32 idx, u32 bindingSlot)
{
    if (GetRef() == VK_NULL_HANDLE)
    {
        DEBUG_LOG_ERR("DescriptorSetVulkan: Attempted WriteSamplerUpdate on VK_NULL_HANDLE descriptor set");
        return;
    }

    VkDescriptorImageInfo samplerInfo{};
    samplerInfo.sampler = sampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = GetRef();
    descriptorWrite.dstBinding = bindingSlot;
    descriptorWrite.dstArrayElement = idx;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &samplerInfo;

    vkUpdateDescriptorSets(VkBackend::Device(), 1, &descriptorWrite, 0, nullptr);
}

void DescriptorSetVulkan::WriteBindlessImageUpdate(const TextureVulkan* pTex, u32 idx, u32 bindingSlot)
{
    if (GetRef() == VK_NULL_HANDLE)
    {
        DEBUG_LOG_ERR("DescriptorSetVulkan: Attempted WriteBindlessImageUpdate on VK_NULL_HANDLE descriptor set");
        return;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.imageView = pTex->GetImageView();
    imageInfo.sampler = VK_NULL_HANDLE;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = GetRef();
    descriptorWrite.dstBinding = bindingSlot == 0 ? m_bindingSlot : bindingSlot;
    descriptorWrite.dstArrayElement = idx;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;
    descriptorWrite.pBufferInfo = nullptr;
    descriptorWrite.pTexelBufferView = nullptr;

    vkUpdateDescriptorSets(VkBackend::Device(), 1, &descriptorWrite, 0, nullptr);
}

void DescriptorPoolVulkan::NamingCallBack(const stltype::string& name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_DESCRIPTOR_POOL;
    nameInfo.objectHandle = (uint64_t)m_descriptorPool;
    nameInfo.pObjectName = name.c_str();

    vkSetDebugUtilsObjectName(VkBackend::Device(), &nameInfo);
}

void DescriptorSetVulkan::NamingCallBack(const stltype::string& name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET;
    nameInfo.objectHandle = (uint64_t)m_descriptorSet;
    nameInfo.pObjectName = name.c_str();

    vkSetDebugUtilsObjectName(VkBackend::Device(), &nameInfo);
}
