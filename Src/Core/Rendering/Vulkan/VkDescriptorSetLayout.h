#pragma once
#include "BackendDefines.h"

// Merely a wrapper for VkDescriptorSetLayout
class DescriptorSetLayoutVulkan : public TrackedResource
{
public:
	DescriptorSetLayoutVulkan() {}
	DescriptorSetLayoutVulkan(VkDescriptorSetLayout& layout) : m_descriptorSetLayout(layout) {}

	~DescriptorSetLayoutVulkan() 
	{
		TRACKED_DESC_IMPL
	}

	virtual void CleanUp() override;

	const VkDescriptorSetLayout& GetRef() const { return m_descriptorSetLayout; }

private:
	VkDescriptorSetLayout m_descriptorSetLayout{ VK_NULL_HANDLE };
};
