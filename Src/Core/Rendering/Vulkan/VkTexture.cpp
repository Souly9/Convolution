#include "VkGlobals.h"
#include "VkGPUMemoryManager.h"
#include "Utils/VkEnumHelpers.h"
#include "VkTexture.h"

TextureVulkan::TextureVulkan()
{
}

TextureVulkan::TextureVulkan(const VkImageCreateInfo& createInfo, const TextureInfo& info) : Tex(info)
{
	const auto device = VK_LOGICAL_DEVICE;
	DEBUG_ASSERT(vkCreateImage(device, &createInfo, VulkanAllocator(), &m_image) == VK_SUCCESS);
	m_imageMemory = g_pGPUMemoryManager->AllocateImage(createInfo, m_image);
	//VkMemoryRequirements memRequirements;
	//vkGetImageMemoryRequirements(device, m_image, &memRequirements);
	//m_imageMemory = g_pGPUMemoryManager->AllocateMemory(memRequirements.size, Conv2MemFlags(BufferUsage::Texture), memRequirements);

}

TextureVulkan::TextureVulkan(const TextureInfo& info) : Tex(info)
{
}

TextureVulkan::~TextureVulkan()
{
	TRACKED_DESC_IMPL
}

void TextureVulkan::CleanUp()
{
	VK_FREE_IF(m_image, vkDestroyImage(VK_LOGICAL_DEVICE, m_image, VulkanAllocator()));
	VK_FREE_IF(m_imageView, vkDestroyImageView(VK_LOGICAL_DEVICE, m_imageView, VulkanAllocator()));
	VK_FREE_IF(m_sampler, vkDestroySampler(VK_LOGICAL_DEVICE, m_sampler, VulkanAllocator()));
	VK_FREE_IF(m_imageMemory, g_pGPUMemoryManager->TryFreeMemory(m_imageMemory));
}

void TextureVulkan::SetImageView(VkImageView view)
{
	VK_FREE_IF(m_imageView, vkDestroyImageView(VK_LOGICAL_DEVICE, m_imageView, VulkanAllocator()));
	m_imageView = view;
}

void TextureVulkan::SetSampler(VkSampler sampler)
{
	VK_FREE_IF(m_sampler, vkDestroySampler(VK_LOGICAL_DEVICE, m_sampler, VulkanAllocator()));
	m_sampler = sampler;
}

void TextureVulkan::NamingCallBack(const stltype::string& name)
{
	VkDebugUtilsObjectNameInfoEXT nameInfo = {};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
	nameInfo.objectHandle = (uint64_t)GetImage();
	nameInfo.pObjectName = name.c_str();

	vkSetDebugUtilsObjectName(VK_LOGICAL_DEVICE, &nameInfo);
}

