#include "VkTexture.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/GPUMemoryManager.h"
#include <utility>
#include <type_traits>
#include "Utils/VkEnumHelpers.h"
#include "VkGPUMemoryManager.h"
#include "VkBackendAccess.h"

TextureVulkan::TextureVulkan()
{
}

TextureVulkan::TextureVulkan(const VkImageCreateInfo& createInfo, const TextureInfo& info) : TextureBase(info)
{
    m_imageMemory = g_renderer.GetGPUMemoryManager().AllocateImage(createInfo, m_image);
}

TextureVulkan::TextureVulkan(const TextureInfo& info) : TextureBase(info)
{
}

TextureVulkan::~TextureVulkan()
{
    TRACKED_DESC_IMPL
}

void TextureVulkan::CleanUp()
{
    VK_FREE_IF(m_imageMemory, g_renderer.GetGPUMemoryManager().TryFreeMemory(m_imageMemory));
    VK_FREE_IF(m_imageView2D, vkDestroyImageView(VkBackend::Device(), m_imageView2D, VulkanAllocator()));
    VK_FREE_IF(m_imageView, vkDestroyImageView(VkBackend::Device(), m_imageView, VulkanAllocator()));
    VK_FREE_IF(m_sampler, vkDestroySampler(VkBackend::Device(), m_sampler, VulkanAllocator()));
    m_image = VK_NULL_HANDLE;
}

void TextureVulkan::SetImageView(VkImageView view)
{
    VK_FREE_IF(m_imageView, vkDestroyImageView(VkBackend::Device(), m_imageView, VulkanAllocator()));
    m_imageView = view;
}

void TextureVulkan::SetImageView2D(VkImageView view2D)
{
    VK_FREE_IF(m_imageView2D, vkDestroyImageView(VkBackend::Device(), m_imageView2D, VulkanAllocator()));
    m_imageView2D = view2D;
}

void TextureVulkan::SetSampler(VkSampler sampler)
{
    VK_FREE_IF(m_sampler, vkDestroySampler(VkBackend::Device(), m_sampler, VulkanAllocator()));
    m_sampler = sampler;
}

void TextureVulkan::NamingCallBack(const stltype::string& name)
{
    VkDebugUtilsObjectNameInfoEXT nameInfo = {};
    nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
    nameInfo.objectHandle = (uint64_t)GetImage();
    nameInfo.pObjectName = name.c_str();
    vkSetDebugUtilsObjectName(VkBackend::Device(), &nameInfo);

    if (m_imageView != VK_NULL_HANDLE)
    {
        VkDebugUtilsObjectNameInfoEXT viewNameInfo = {};
        viewNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        viewNameInfo.objectType = VK_OBJECT_TYPE_IMAGE_VIEW;
        viewNameInfo.objectHandle = (uint64_t)m_imageView;
        stltype::string viewName = name + "_View";
        viewNameInfo.pObjectName = viewName.c_str();
        vkSetDebugUtilsObjectName(VkBackend::Device(), &viewNameInfo);
    }

    if (m_sampler != VK_NULL_HANDLE)
    {
        VkDebugUtilsObjectNameInfoEXT samplerNameInfo = {};
        samplerNameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        samplerNameInfo.objectType = VK_OBJECT_TYPE_SAMPLER;
        samplerNameInfo.objectHandle = (uint64_t)m_sampler;
        stltype::string samplerName = name + "_Sampler";
        samplerNameInfo.pObjectName = samplerName.c_str();
        vkSetDebugUtilsObjectName(VkBackend::Device(), &samplerNameInfo);
    }
}

