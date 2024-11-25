#include "Core/Global/GlobalDefines.h"
#include "VkDescriptorSetLayout.h"
#include "VkGlobals.h"


void DescriptorSetLayoutVulkan::CleanUp()
{
	VK_FREE_IF(m_descriptorSetLayout, vkDestroyDescriptorSetLayout(VK_LOGICAL_DEVICE, m_descriptorSetLayout, VulkanAllocator()));
}