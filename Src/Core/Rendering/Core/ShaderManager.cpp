#include "ShaderManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/IO/FileReader.h"
#include "Utils/ShaderCompiler.h"
// Using the normal filesystem lib here for simplicity
#include <filesystem>

namespace fs = std::filesystem;

void ShaderManager::CleanShaderOutputDirectory()
{
    const stltype::string outputPath = SHADER_OUTPUT_DIRECTORY;

    if (fs::exists(outputPath.c_str()))
    {
        DEBUG_LOGF("Deleting all files in the shader output directory {}", outputPath.c_str());
        std::uintmax_t items_removed = fs::remove_all(outputPath.c_str());

        DEBUG_LOGF("Deleted {} files", items_removed);
    }
    else
    {
        DEBUG_LOGF("Shader output path \"{}\" does not exist in the exe output directory", outputPath.c_str());
    }
}

bool ShaderManager::ReadAllSourceShaders()
{
    ScopedZone("ShaderManager::ReadAllSourceShaders");
    m_compiler.UpdateIncluders(SHADER_INCLUDE_DIRECTORY);
    m_compiler.Reset();
    m_compiledShaders.clear();
    m_totalShaderFiles = 0;
    m_readShaderFiles = 0;

    const stltype::string shaderPath = SHADER_FILES_DIRECTORY;
    for (const auto& entry : fs::recursive_directory_iterator(shaderPath.c_str()))
    {
        if (entry.is_regular_file())
        {
            ++m_totalShaderFiles;

            stltype::string path(entry.path().string().c_str());
            const stltype::string extension = entry.path().extension().string().c_str();
            stltype::string filename = entry.path().filename().string().c_str();

            path = path.replace(path.find("\\"), 1, "/");
            DEBUG_LOGF("Discovered shader file {}", path.c_str());

            ShaderTypeBits shaderType;
            if (extension == ".vert")
            {
                shaderType = ShaderTypeBits::Vertex;
            }
            else if (extension == ".frag")
            {
                shaderType = ShaderTypeBits::Fragment;
            }
            else if (extension == ".comp")
            {
                shaderType = ShaderTypeBits::Compute;
            }
            else
            {
                DEBUG_ASSERT(false);
            }
            IORequest req{.filePath = path,
                          .callback =
                              [this, shaderType](ReadBytesInfo& data)
                          {
                              m_compiler.AddShaderCode(shaderType, data.filePath, std::move(data.bytes));
                              ++m_readShaderFiles;
                          },
                          .requestType = RequestType::Bytes

            };
            g_pFileReader->SubmitIORequest(req);
        }
    }
    return CompileAllShaders();
}

bool ShaderManager::CompileAllShaders()
{
    while (m_readShaderFiles != m_totalShaderFiles)
    {
    }
    DEBUG_LOGF("Compiling {} shader files", m_totalShaderFiles);
    m_compiledShaders = m_compiler.CompileAllShaders();
    // Only get filled map when compilation was successful
    return m_compiledShaders.size() > 0;
}

bool ShaderManager::ReloadAllShaders()
{
    DEBUG_LOGF("Reloading {} shader files", m_totalShaderFiles);
    // CleanShaderOutputDirectory();
    return ReadAllSourceShaders();
}

const SpirVBinary& ShaderManager::GetShader(const stltype::string_view& relativePath)
{
    if (const auto it = m_compiledShaders.find(relativePath.data()); it != m_compiledShaders.end())
    {
        return it->second;
    }
    DEBUG_ASSERT(false);
    return {};
}
