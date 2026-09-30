#pragma once
#include "MtlBackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Shader.h"

// One MTL::Function from a precompiled .metallib (or MSL source at runtime for hot reload)
class ShaderMetal : public ShaderBase
{
public:
    ShaderMetal(const stltype::string_view& filePath, stltype::string&& name);
    ShaderMetal(const char* filePath, const char* name);
    ~ShaderMetal();

    MTL::Function* GetDesc() const;
    const stltype::string& GetName() const;

private:
    void CreateFunction(const stltype::string_view& filePath);

    MTL::Library* m_library{nullptr};
    MTL::Function* m_function{nullptr};

    stltype::string m_name;
};
