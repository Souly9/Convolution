#include "VkShader.h"
#include "BackendDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/IO/FileReader.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "VkGlobals.h"
#include <vulkan/vulkan.h>

ShaderVulkan::ShaderVulkan(const stltype::string_view& filePath, stltype::string&& name) : m_name{name}
{
    const auto shaderData = g_pShaderManager->GetShader(filePath);
    DEBUG_LOGF("Creating shader for {}", filePath.data());
    CreateShaderModule(shaderData.words);
    /*IORequest req{};
    req.filePath = filePath;
    req.requestType = RequestType::Bytes;
    req.callback = IOByteReadCallback([this](const ReadBytesInfo& result)
        {
            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = result.bytes.size();
            createInfo.pCode = (const u32*)result.bytes.data();

            DEBUG_ASSERT(vkCreateShaderModule(VkGlobals::GetLogicalDevice(), &createInfo, VulkanAllocator(),
    &m_shaderModule) == VK_SUCCESS);
        });
    g_pFileReader->SubmitIORequest(req);*/
}

ShaderVulkan::ShaderVulkan(const char* filePath, const char* name)
    : ShaderVulkan{stltype::string(filePath), stltype::string(name)}
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

void ShaderVulkan::CreateShaderModule(const stltype::vector<u32>& byteCode)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = byteCode.size() * sizeof(u32);
    createInfo.pCode = byteCode.data();

    DEBUG_ASSERT(vkCreateShaderModule(VkGlobals::GetLogicalDevice(), &createInfo, VulkanAllocator(), &m_shaderModule) ==
                 VK_SUCCESS);
}
