#include "VkShader.h"
#include <vulkan/vulkan.h>
#include "Core/IO/FileReader.h"
#include "BackendDefines.h"
#include "VkGlobals.h"

ShaderVulkan::ShaderVulkan(const stltype::string_view& filePath, stltype::string&& name) : m_name{ name }
{
	IORequest req{};
	req.filePath = filePath;
	req.requestType = RequestType::Bytes;
	req.callback = IOByteReadCallback([this](const ReadBytesInfo& result)
		{
			CreateShaderModule(result.bytes);
		});
	g_pFileReader->SubmitIORequest(req);
}

ShaderVulkan::ShaderVulkan(const char* filePath, const char* name) : ShaderVulkan{ stltype::string(filePath), stltype::string(name) }
{
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

void ShaderVulkan::CreateShaderModule(const stltype::vector<char>& byteCode)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = byteCode.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(byteCode.data());

	DEBUG_ASSERT(vkCreateShaderModule(VkGlobals::GetLogicalDevice(), &createInfo, VulkanAllocator(), &m_shaderModule) == VK_SUCCESS);
}
