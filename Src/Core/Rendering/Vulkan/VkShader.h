#pragma once
#include "Core/Global/GlobalDefines.h"
#include "BackendDefines.h"

class ShaderVulkan
{
public:
	ShaderVulkan(const stltype::string_view& filePath, stltype::string&& name);
	ShaderVulkan(const char* filePath, const char* name);
	~ShaderVulkan();

	VkShaderModule GetDesc() const;
	const stltype::string& GetName() const;

private:
	void CreateShaderModule(const stltype::vector<char>& byteCode);

	VkShaderModule m_shaderModule{};

	stltype::string m_name;
};
