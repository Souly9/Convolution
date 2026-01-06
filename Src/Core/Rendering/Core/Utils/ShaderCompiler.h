#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/ThreadBase.h"
#include "Core/Rendering/Core/Defines/DescriptorLayoutDefines.h"

struct SpirVBinary
{
    stltype::vector<u32> words;
};

using ShaderMap = stltype::hash_map<stltype::string, SpirVBinary>;
using IncluderMap = stltype::hash_map<stltype::string, stltype::vector<char>>;

// Simple shader compiler class to compile shaders from HLSL to SPIR-V using glslang at runtime for shader hot-reloading
// mainly
class ShaderCompiler
{
public:
    // Compiles all shaders in the "Shaders" directory, returns false if any failed
    // Uses the standard extensions for shader files, standard "main" as entrypoint and so on
    ShaderMap CompileAllShaders();

    void AddShaderCode(ShaderTypeBits type, const stltype::string& fileName, stltype::vector<char>&& contents);

    void UpdateIncluders(const stltype::string& includerRootDir);

    void Reset();

private:
    stltype::string BuildOutputFileName(const stltype::string& fileName);

    struct ShaderCompileData
    {
        ShaderTypeBits type;
        stltype::string fileName;
        stltype::vector<char> contents;
    };
    stltype::vector<ShaderCompileData> m_compileData;

    ProfiledLockable(CustomMutex, m_dataFutex);
};