#include "MtlShader.h"

// TODO(Metal): load .metallib via MTL::Device::newLibrary and pick the entry point

ShaderMetal::ShaderMetal(const stltype::string_view& filePath, stltype::string&& name) : m_name(std::move(name))
{
    CreateFunction(filePath);
}

ShaderMetal::ShaderMetal(const char* filePath, const char* name) : m_name(name)
{
    CreateFunction(filePath);
}

ShaderMetal::~ShaderMetal()
{
}

MTL::Function* ShaderMetal::GetDesc() const
{
    return m_function;
}

const stltype::string& ShaderMetal::GetName() const
{
    return m_name;
}

void ShaderMetal::CreateFunction(const stltype::string_view& filePath)
{
}
