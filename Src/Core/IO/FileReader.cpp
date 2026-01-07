#define STB_IMAGE_IMPLEMENTATION
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <filesystem>
#include <fstream>
#undef abs
#define STBI_NO_SIMD
#include <stb/stb_image.h>

#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"
#include "FileReader.h"
#include "MeshConverter.h"

using namespace threadstl;

FileReader::FileReader()
{
    m_ioThread = MakeThread([this]() { CheckIORequests(); });
    m_ioThread.SetName("Convolution_IO");
}

FileReader::~FileReader()
{
    m_keepRunning = false;
    m_ioThread.WaitForEnd();
}

void FileReader::FinishAllRequests()
{
    while (m_requests.empty() == false)
    {
        Sleep(1);
    }
}

void FileReader::CancelAllRequests()
{
    m_requestSubmitMutex.Lock();
    for (int i = 0; i < m_requests.size(); ++i)
    {
        m_requests.pop();
    }
    m_requestSubmitMutex.Unlock();
}

void FileReader::SubmitIORequest(const IORequest& request)
{
    m_requestSubmitMutex.Lock();
    m_requests.push(request);
    m_requestSubmitMutex.Unlock();
}

void FileReader::CheckIORequests()
{
    while (m_keepRunning)
    {
        if (m_requests.empty())
        {
            threadstl::ThreadSleep(50);
            continue;
        }

        // Copy and pop request while holding lock, then release before processing
        // This avoids deadlock when mesh loading triggers texture IO requests
        m_requestSubmitMutex.Lock();
        const IORequest request = m_requests.front();
        m_requests.pop();
        m_requestSubmitMutex.Unlock();

        switch (request.requestType)
        {
            case RequestType::Bytes:
            {
                ReadFileAsGenericBytes(request);
                break;
            }
            case RequestType::Image:
            {
                ReadImageFile(request);
                break;
            }
            case RequestType::Mesh:
            {
                ReadMeshFile(request);
                break;
            }
            default:
                DEBUG_ASSERT(false);
                break;
        }
    }
}

void FileReader::ReadFileAsGenericBytes(const IORequest& request)
{
    ReadBytesInfo info{};
    info.bytes = ReadFileAsGenericBytes(request.filePath.data());
    info.filePath = request.filePath;

    const IOByteReadCallback* callback = stltype::get_if<IOByteReadCallback>(&request.callback);
    if (callback)
        (*callback)(info);
}

stltype::vector<char> FileReader::ReadFileAsGenericBytes(const char* filePath)
{
    ScopedZone("FileReader::Read File As Generic Bytes");
    std::ifstream fileStream(filePath, std::ios::ate | std::ios::binary);

    DEBUG_ASSERT(fileStream.is_open());

    size_t fileSize = (size_t)fileStream.tellg();
    DEBUG_ASSERT(fileSize > 0);

    stltype::vector<char> buffer(fileSize);

    fileStream.seekg(0);
    fileStream.read(buffer.data(), fileSize);

    fileStream.close();
    return buffer;
}

void FileReader::ReadImageFile(const IORequest& request)
{
    ScopedZone("FileReader::Read Image File");
    ReadTextureInfo info{};
    info.pixels =
        stbi_load(request.filePath.data(), &info.extents.x, &info.extents.y, &info.texChannels, STBI_rgb_alpha);
    DEBUG_ASSERT(info.pixels);
    info.filePath = std::move(request.filePath);

    const IOImageReadCallback* callback = stltype::get_if<IOImageReadCallback>(&request.callback);
    if (callback)
        (*callback)(info);
}

void FileReader::FreeImageData(unsigned char* pixels)
{
    stbi_image_free(pixels);
}

void FileReader::ReadMeshFile(const IORequest& request)
{
    ScopedZone("FileReader::Read Mesh File");
#define AI_CONFIG_PP_RVC_FLAGS aiComponent::aiComponent_COLORS | aiComponent::aiComponent_CAMERAS

    Assimp::Importer importer;
    stltype::string_view path = request.filePath;
    const auto ext = path.substr(path.find_last_of('.'));
    DEBUG_ASSERT(importer.IsExtensionSupported(ext.data()));
    const aiScene* pMeshScene =
        importer.ReadFile(path.data(),
                          aiProcess_ConvertToLeftHanded | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
                              aiProcess_RemoveComponent | aiProcess_RemoveRedundantMaterials | aiProcess_GenUVCoords |
                              aiProcess_GenBoundingBoxes | aiProcess_GenSmoothNormals);

    DEBUG_ASSERT(pMeshScene);
    auto scene = MeshConversion::Convert(pMeshScene);

    const IOMeshReadCallback* callback = stltype::get_if<IOMeshReadCallback>(&request.callback);
    if (callback)
        (*callback)({scene});
}
