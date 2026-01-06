#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Utils/ShaderCompiler.h"

// Relative to exe
#define SHADER_OUTPUT_DIRECTORY "/Shaders/"
// Not portable at all but we should only compile shaders when deving locally either way
#define SHADER_FILES_DIRECTORY   "../Shaders/Uncompiled/"
#define SHADER_INCLUDE_DIRECTORY "../Shaders/Globals/"

class ShaderManager
{
public:
    void CleanShaderOutputDirectory();
    bool ReadAllSourceShaders();
    bool CompileAllShaders();

    bool ReloadAllShaders();

    // Expects a path to the spirv file relative to the exe
    // Stalls if it needs to read the shader but should not happen
    const SpirVBinary& GetShader(const stltype::string_view& relativePath);

private:
    // Not the fastest but okay enough
    ShaderMap m_compiledShaders;
    ShaderCompiler m_compiler;
    threadstl::AtomicUint32 m_readShaderFiles;
    u32 m_totalShaderFiles;
};