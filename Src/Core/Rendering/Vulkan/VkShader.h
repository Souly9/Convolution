#pragma once
#include "Core/Global/GlobalDefines.h"
#include "BackendDefines.h"
#include "Core/Rendering/Core/Shader.h"

template<>
class ShaderImpl<Vulkan>
{
public:
	ShaderImpl(const stltype::string_view& filePath, stltype::string&& name);
	ShaderImpl(const char* filePath, const char* name);
	~ShaderImpl();

	VkShaderModule GetHandle() const;
	const stltype::string& GetName() const;

private:
	void CreateShaderModule(const char* filePath);

	VkShaderModule m_shaderModule;

	stltype::string m_name;
};
