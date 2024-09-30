#include "VkShader.h"
#include <vulkan/vulkan.h>
#include "Core/IO/FileReader.h"
#include "BackendDefines.h"
#include "VkGlobals.h"

ShaderVulkan::ShaderVulkan(const stltype::string_view& filePath, stltype::string&& name) : m_name{ name }
{
	CreateShaderModule(filePath.data());
}

ShaderVulkan::ShaderVulkan(const char* filePath, const char* name) : m_name{ name }
{
	CreateShaderModule(filePath);
}

ShaderVulkan::~ShaderVulkan()
{
	vkDestroyShaderModule(VkGlobals::GetLogicalDevice(), m_shaderModule, VulkanAllocator());
}

VkShaderModule ShaderVulkan::GetDesc() const
{
	return m_shaderModule;
}

const stltype::string& ShaderVulkan::GetName() const
{
	return m_name;
}

void ShaderVulkan::CreateShaderModule(const char* filePath)
{
	const auto byteCode = FileReader::ReadFileAsGenericBytes(filePath);

	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = byteCode.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(byteCode.data());

	DEBUG_ASSERT(vkCreateShaderModule(VkGlobals::GetLogicalDevice(), &createInfo, VulkanAllocator(), &m_shaderModule) == VK_SUCCESS);
}
