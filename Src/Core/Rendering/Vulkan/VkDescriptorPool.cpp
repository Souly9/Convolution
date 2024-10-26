#include "VkDescriptorPool.h"
#include "VkGlobals.h"


DescriptorPoolVulkan::DescriptorPoolVulkan()
{
}

DescriptorPoolVulkan::~DescriptorPoolVulkan()
{
	vkDestroyDescriptorPool(VK_LOGICAL_DEVICE, m_descriptorPool, VulkanAllocator());
}

void DescriptorPoolVulkan::Create(const DescriptorPoolCreateInfo& createInfo)
{
	stltype::vector<VkDescriptorPoolSize> poolSizes;

	VkDescriptorPoolSize poolSizeUBO{};
	poolSizeUBO.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizeUBO.descriptorCount = MAX_DESCRIPTOR_SETS;
	poolSizes.push_back(poolSizeUBO);

	if (createInfo.enableBindlessTextureDescriptors)
	{
		VkDescriptorPoolSize poolSizeBindlessTextures{};
		poolSizeBindlessTextures.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizeBindlessTextures.descriptorCount = MAX_BINDLESS_TEXTURES;
		poolSizes.push_back(poolSizeBindlessTextures);
	}

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;

	if (createInfo.enableBindlessTextureDescriptors)
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;

	poolInfo.poolSizeCount = poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = MAX_DESCRIPTOR_SETS;

	DEBUG_ASSERT(vkCreateDescriptorPool(VK_LOGICAL_DEVICE, &poolInfo, VulkanAllocator(), &m_descriptorPool) == VK_SUCCESS);
}

stltype::vector<DescriptorSetVulkan*> DescriptorPoolVulkan::CreateDescriptorSetsUBO(const stltype::vector<VkDescriptorSetLayout>& layouts)
{
	DEBUG_ASSERT(m_descriptorSetCount + layouts.size() < MAX_DESCRIPTOR_SETS);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = layouts.size();
	allocInfo.pSetLayouts = layouts.data();
	
	stltype::vector<VkDescriptorSet> descriptorSets;
	descriptorSets.resize(layouts.size());
	DEBUG_ASSERT(vkAllocateDescriptorSets(VK_LOGICAL_DEVICE, &allocInfo, descriptorSets.data()) == VK_SUCCESS);

	stltype::vector<DescriptorSetVulkan*> rslt;
	rslt.reserve(layouts.size());
	for (u32 i = 0; i < descriptorSets.size(); ++i)
	{
		m_createdDescriptorSets[m_descriptorSetCount + i] = DescriptorSetVulkan(descriptorSets[i]);
		rslt.push_back(&m_createdDescriptorSets.at(m_descriptorSetCount + i));
	}
	m_descriptorSetCount += layouts.size() - 1;

	return rslt;
}

DescriptorSetVulkan* DescriptorPoolVulkan::CreateDescriptorSet(VkDescriptorSetLayout& layout)
{
	DEBUG_ASSERT(m_descriptorSetCount + 1 < MAX_DESCRIPTOR_SETS);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;

	VkDescriptorSet descriptorSet;
	DEBUG_ASSERT(vkAllocateDescriptorSets(VK_LOGICAL_DEVICE, &allocInfo, &descriptorSet) == VK_SUCCESS);

	auto& set = m_createdDescriptorSets[m_descriptorSetCount] = DescriptorSetVulkan(descriptorSet);

	++m_descriptorSetCount;

	return &set;
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

void DescriptorSetVulkan::WriteBufferUpdate(const GenericBuffer& buffer)
{
	WriteBufferUpdate(buffer, buffer.GetInfo().size, 0);
}

void DescriptorSetVulkan::WriteBufferUpdate(const GenericBuffer& buffer, u32 size, u32 offset)
{
	DEBUG_ASSERT(m_bindingSlot != 0);

	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = buffer.GetRef();
	bufferInfo.offset = offset;
	bufferInfo.range = size;

	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = GetRef();
	descriptorWrite.dstBinding = m_bindingSlot;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorWrite.descriptorCount = 1; 
	descriptorWrite.pBufferInfo = &bufferInfo;
	descriptorWrite.pImageInfo = nullptr; // Optional
	descriptorWrite.pTexelBufferView = nullptr; // Optional

	vkUpdateDescriptorSets(VK_LOGICAL_DEVICE, 1, &descriptorWrite, 0, nullptr);
}

void DescriptorSetVulkan::WriteBindlessTextureUpdate(const TextureVulkan* pTex, u32 idx)
{
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageInfo.imageView = pTex->GetImageView();
	imageInfo.sampler = pTex->GetSampler();

	VkWriteDescriptorSet descriptorWrite{};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = GetRef();
	descriptorWrite.dstBinding = m_bindingSlot;
	descriptorWrite.dstArrayElement = idx;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;
	descriptorWrite.pBufferInfo = nullptr; // Optional
	descriptorWrite.pTexelBufferView = nullptr; // Optional

	vkUpdateDescriptorSets(VK_LOGICAL_DEVICE, 1, &descriptorWrite, 0, nullptr);
}
