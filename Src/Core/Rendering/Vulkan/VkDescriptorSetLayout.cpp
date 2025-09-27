#include "Core/Global/GlobalDefines.h"
#include "VkDescriptorSetLayout.h"
#include "VkGlobals.h"


void DescriptorSetLayoutVulkan::CleanUp()
{
	VK_FREE_IF(m_descriptorSetLayout, vkDestroyDescriptorSetLayout(VK_LOGICAL_DEVICE, m_descriptorSetLayout, VulkanAllocator()));
}

void DescriptorSetLayoutVulkan::NamingCallBack(const stltype::string& name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT;
    nameInfo.objectHandle = (uint64_t)GetRef();
    nameInfo.pObjectName = name.c_str();

    vkSetDebugUtilsObjectName(VK_LOGICAL_DEVICE, &nameInfo);
}
