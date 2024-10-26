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

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, m_image, &memRequirements);

	m_imageMemory = g_pGPUMemoryManager->AllocateMemory(info.size, Conv2MemFlags(BufferUsage::Texture), memRequirements);


	vkBindImageMemory(device, m_image, m_imageMemory, 0);
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
	VK_FREE_IF(m_imageView, g_pGPUMemoryManager->TryFreeMemory(m_imageMemory));
}

