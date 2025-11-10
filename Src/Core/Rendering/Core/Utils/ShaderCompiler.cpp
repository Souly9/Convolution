#include "Core/Global/GlobalDefines.h"
#include "ShaderCompiler.h"
#include "../ShaderManager.h"
#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <filesystem>

namespace fs = std::filesystem;

// Love having to handle includes myself
class GlslangIncluder : public glslang::TShader::Includer
{
public:
    // Implement the logic to find and load the included file content
    virtual IncludeResult* includeSystem(const char* headerName,
        const char* includerName,
        size_t inclusionDepth) override
    {
        if (headerName == nullptr || includerName == nullptr)
        {
            return nullptr;
        }

        if (m_readShaderFiles > 0)
        {
            g_pFileReader->FinishAllRequests();
        }

        stltype::string fileName = headerName;
        fileName = fileName.substr(fileName.find_last_of('/') + 1);
        const auto& pathInfo = std::find_if(m_includerPaths.begin(), m_includerPaths.end(), [&](const IncluderPathInfo& info)
            {
                return info.fileName == fileName;
            });
        const auto it = m_includerMap.find(pathInfo->absolutePath);
        DEBUG_ASSERT(it != m_includerMap.end());

        auto& rslt = m_includerResults.emplace_back(it->first.c_str(), it->second.data(), it->second.size(), nullptr);

        return &rslt;
    }

    virtual IncludeResult* includeLocal(const char* headerName,
        const char* includerName,
        size_t inclusionDepth) override
    {
        return nullptr;
    }

    // Implement the cleanup function
    virtual void releaseInclude(IncludeResult* result) override
    {
        // will just free the whole vector after compile
    }

    void ReadAllFiles(const std::string& rootDir)
    {
        ScopedZone("GlslangIncluder::ReadAllFiles");
        m_includerResults.clear();
        m_includerResults.reserve(150);
        for (const auto& entry : fs::recursive_directory_iterator(rootDir.c_str()))
        {

            if (entry.is_regular_file())
            {
                ++m_readShaderFiles;
                stltype::string absolutePath;
                if (entry.path().is_absolute() == false)
                {
                    absolutePath = fs::absolute(entry.path()).string().c_str();
                }
                else
                {
					absolutePath = entry.path().string().c_str();
                }
                stltype::string fileName = entry.path().filename().string().c_str();
                DEBUG_LOGF("Discovered include file {}", absolutePath.c_str());

                IORequest req
                {
                    .filePath = absolutePath,
                    .callback = [this, fileName](ReadBytesInfo& data)
                    {
                        m_includerMap[data.filePath] = data.bytes;
                        m_includerPaths.emplace_back(fileName, data.filePath);
                        --m_readShaderFiles;
                    },
                    .requestType = RequestType::Bytes

                };
                g_pFileReader->SubmitIORequest(req);
            }
        }
    }
private:
    IncluderMap m_includerMap;
    struct IncluderPathInfo
    {
		stltype::string fileName;
		stltype::string absolutePath;
    };
    stltype::vector<IncluderPathInfo> m_includerPaths;
    stltype::vector<IncludeResult> m_includerResults;
    threadstl::AtomicUint32 m_readShaderFiles = 0;
};

GlslangIncluder s_includer;

static SpirVBinary CompileShader(EShLanguage type, const stltype::vector<char>& shaderSource, const char* fileName)
{
    ScopedZone("ShaderCompiler::CompileShader");
    DEBUG_LOGF("Compiling shader {}", fileName);


    glslang::TShader shader(type);
    int lengths[] = { (int)shaderSource.size() };
    auto source = reinterpret_cast<const char*>(shaderSource.data());
    shader.setStringsWithLengths(&source, lengths, 1);

    shader.setEnvClient(
        glslang::EShClientVulkan,       
        glslang::EShTargetVulkan_1_4
    );

    shader.setEnvTarget(
        glslang::EShTargetSpv,         
        glslang::EShTargetSpv_1_6
    ); 

    EShMessages messages = (EShMessages)(EShMsgDefault | EShMsgVulkanRules | EShMsgSpvRules);
    
    if (shader.parse(
        GetDefaultResources(),
        100,
        ENoProfile,
        false,
        true,
        messages,
        s_includer
    ) == false)
    {
        const auto pLog = shader.getInfoLog();
        DEBUG_LOGF("Shader compilation failed: {}", pLog);
        return {};
    }
    DEBUG_LOGF("Shader compilation succeeded!");

    glslang::TProgram program;
    program.addShader(&shader);

    // Link the program
    if (!program.link(EShMsgVulkanRules))
    {
        DEBUG_LOGF("Shader linking failed: {}", program.getInfoLog());
        DEBUG_LOGF("Shader linking debug log: {}", program.getInfoDebugLog());
        return {};
    }
	DEBUG_LOGF("Shader linking succeeded!");
    const auto inter = shader.getIntermediate();

    std::vector<u32> tmp;
    spv::SpvBuildLogger logger;

    // 6. Convert the IR to SPIR-V
    glslang::GlslangToSpv(*inter, tmp, &logger, nullptr);

    SpirVBinary rslt;
    rslt.words.reserve(tmp.size());
    std::copy(tmp.begin(), tmp.end(), std::back_inserter(rslt.words));

    if (!logger.getAllMessages().empty())
    {
        DEBUG_LOGF("SPIR-V conversion failed: {}", logger.getAllMessages());
        return {};
    }

    if (rslt.words.empty())
    {
		DEBUG_LOGF("SPIR-V conversion failed: {}", logger.getAllMessages());
        return {};
    }

    return rslt;
}

ShaderMap ShaderCompiler::CompileAllShaders()
{
    ScopedZone("ShaderCompiler::CompileAllShaders");

    m_dataFutex.lock();
    glslang::InitializeProcess();
    ShaderMap rsltMap;
    for (auto& data : m_compileData)
    {
        EShLanguage type;
		switch (data.type)
		{
		case ShaderTypeBits::Vertex:
			type = EShLangVertex;
			break;
		case ShaderTypeBits::Fragment:
			type = EShLangFragment;
			break;
		case ShaderTypeBits::Compute:
			type = EShLangCompute;
			break;
        default:
			DEBUG_ASSERT(false);
			break;
		}
        const auto spirv = CompileShader(type, data.contents, data.fileName.c_str());
        if (spirv.words.empty())
		{
			DEBUG_LOGF("Shader compilation failed: {}", data.fileName);
            m_dataFutex.unlock();
            return {};
		}
        rsltMap.emplace(BuildOutputFileName(data.fileName), spirv);
    }
    glslang::FinalizeProcess();
	m_dataFutex.unlock();
	return rsltMap;
}

void ShaderCompiler::AddShaderCode(ShaderTypeBits type, const stltype::string& fileName, stltype::vector<char>&& contents)
{
    m_dataFutex.lock();
    m_compileData.emplace_back(type, fileName, contents);
    m_dataFutex.unlock();
}

void ShaderCompiler::UpdateIncluders(const stltype::string& includerRootDir)
{
    s_includer.ReadAllFiles(includerRootDir.c_str());
}

void ShaderCompiler::Reset()
{
    m_compileData.clear();
}

stltype::string ShaderCompiler::BuildOutputFileName(const stltype::string& fileName)
{
    return "Shaders/" + fileName.substr(fileName.find_last_of('/') + 1) + ".spv";
}
