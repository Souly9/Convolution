#pragma once
#include "Core/Global/GlobalDefines.h"
#include "BackendDefines.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"

static inline constexpr u32 MAX_DESCRIPTOR_SETS = 128;

class DescriptorSetVulkan
{
public:
	DescriptorSetVulkan();
	DescriptorSetVulkan(VkDescriptorSet descriptorSet) : m_descriptorSet(descriptorSet) {}

	void SetBindingSlot(u32 binding);

	VkDescriptorSet GetRef() const;

	// Updates the descriptor set's buffer with the supplied data
	void WriteBufferUpdate(const GenericBuffer& buffer);
	void WriteBufferUpdate(const GenericBuffer& buffer, u32 size, u32 offset = 0);
	void WriteBindlessTextureUpdate(const TextureVulkan* pTex, u32 idx);

private:
	VkDescriptorSet m_descriptorSet{ VK_NULL_HANDLE };
	u32 m_bindingSlot{ 0 };
};

struct DescriptorPoolCreateInfo
{
	bool enableBindlessTextureDescriptors{ true };
};
class DescriptorPoolVulkan
{
public:
	DescriptorPoolVulkan();
	~DescriptorPoolVulkan();

	void Create(const DescriptorPoolCreateInfo& createInfo);

	stltype::vector<DescriptorSetVulkan*> CreateDescriptorSetsUBO(const stltype::vector<VkDescriptorSetLayout>& layouts);
	DescriptorSetVulkan* CreateDescriptorSet(VkDescriptorSetLayout& layouts);

	bool IsValid() const { return m_descriptorPool != VK_NULL_HANDLE; }

protected:
	stltype::array<DescriptorSetVulkan, MAX_DESCRIPTOR_SETS> m_createdDescriptorSets;
	VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
	u32 m_descriptorSetCount = 0;
};