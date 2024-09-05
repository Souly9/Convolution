#include <vulkan/vulkan.h>
#include "Core/IO/FileReader.h"
#include "BackendDefines.h"
#include "VkGlobals.h"
#include "VkShader.h"

ShaderImpl<Vulkan>::ShaderImpl(const stltype::string_view& filePath, stltype::string&& name) : m_name{ name }
{
	CreateShaderModule(filePath.data());
}

ShaderImpl<Vulkan>::ShaderImpl(const char* filePath, const char* name) : m_name{ name }
{
	CreateShaderModule(filePath);
}

ShaderImpl<Vulkan>::~ShaderImpl()
{
	vkDestroyShaderModule(VkGlobals::GetLogicalDevice(), m_shaderModule, VulkanAllocator());
}

VkShaderModule ShaderImpl<Vulkan>::GetHandle() const
{
	return m_shaderModule;
}

const stltype::string& ShaderImpl<Vulkan>::GetName() const
{
	return m_name;
}

void ShaderImpl<Vulkan>::CreateShaderModule(const char* filePath)
{
	const auto byteCode = FileReader::ReadFileAsGenericBytes(filePath);

	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = byteCode.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(byteCode.data());

	DEBUG_ASSERT(vkCreateShaderModule(VkGlobals::GetLogicalDevice(), &createInfo, VulkanAllocator(), &m_shaderModule) == VK_SUCCESS);
}
