#define STB_IMAGE_IMPLEMENTATION
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <filesystem>
#include <fstream>
#undef abs
#define STBI_NO_SIMD
#define TINYDDSLOADER_IMPLEMENTATION
#include <tinyddsloader.h>
#include <stb/stb_image.h>


#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"
#include "FileReader.h"
#include "MeshDecoder.h"
#include "Core/SceneGraph/SceneStreamer.h"

using namespace threadstl;

void FileReader::FreeTextureInfo(const ReadTextureInfo& info)
{
    FileReader::FreeImageData(info.pixels);
    for (const auto& mip : info.mipmapPixels)
        FileReader::FreeImageData(mip.pData);
}

FileReader::FileReader() : m_threadPool(CORE_COUNT_AVAILABLE)
{
}

FileReader::~FileReader()
{
    Stop();
}

void FileReader::Stop()
{
    m_threadPool.WaitAll();
    for (const auto& done : m_completedImages)
        FreeTextureInfo(done.info);
    m_completedImages.clear();
    m_completedMeshes.clear();
}

void FileReader::FinishAllRequests()
{
    m_threadPool.WaitAll();
}

bool FileReader::IsStale(const IORequest& request) const
{
    return !request.isPersistent && request.generation != m_generation;
}

void FileReader::SubmitIORequest(const IORequest& request)
{
    IORequest submitted = request;
    submitted.generation = m_generation;
    if (!Engine::IsWindows())
    {
        // Some asset paths are written with Windows separators
        for (auto& c : submitted.filePath)
            if (c == '\\')
                c = '/';
    }

    switch (submitted.requestType)
    {
        case RequestType::Bytes:
        {
            m_threadPool.Submit([this, submitted]() { ReadFileAsGenericBytes(submitted); });
            break;
        }
        case RequestType::Image:
        {
            m_threadPool.Submit([this, submitted]() { ReadImageFile(submitted); });
            break;
        }
        case RequestType::Mesh:
        {
            m_threadPool.Submit([this, submitted]() { ReadMeshFile(submitted); });
            break;
        }
    }
}

void FileReader::DeliverCompleted(u32 maxImages)
{
    ScopedZone("FileReader::DeliverCompleted");

    stltype::deque<CompletedMesh> meshes;
    stltype::deque<CompletedImage> images;
    stltype::deque<CompletedImage> staleImages;
    {
        SimpleScopedGuard<CustomMutex> lock(m_completedMutex);
        meshes.swap(m_completedMeshes);
        // Results of an older scene are dropped without using up the frame's image budget
        while (!m_completedImages.empty() && images.size() < maxImages)
        {
            auto& front = m_completedImages.front();
            (IsStale(front.request) ? staleImages : images).push_back(stltype::move(front));
            m_completedImages.pop_front();
        }
    }
    m_lastDeliveredImages = (u32)images.size();

    for (const auto& done : staleImages)
        FreeTextureInfo(done.info);
    if (!staleImages.empty())
        DEBUG_LOGF("[FileReader] Dropped {} stale image results", (u32)staleImages.size());

    for (auto& done : meshes)
    {
        if (IsStale(done.request))
        {
            DEBUG_LOGF("[FileReader] Dropping stale mesh result: {}", done.request.filePath.c_str());
            continue;
        }

        if (const auto* callback = stltype::get_if<IOMeshReadCallback>(&done.request.callback))
            g_engine.GetSceneStreamer().Begin(stltype::move(done.pScene), *callback);
    }

    for (auto& done : images)
    {
        if (const auto* callback = stltype::get_if<IOImageReadCallback>(&done.request.callback))
            (*callback)(done.info);
    }
}

void FileReader::ReadFileAsGenericBytes(const IORequest& request)
{
    ReadBytesInfo info{};
    info.bytes = ReadFileAsGenericBytes(request.filePath.data());
    info.filePath = request.filePath;

    const IOByteReadCallback* callback = stltype::get_if<IOByteReadCallback>(&request.callback);
    if (callback)
    {
        SimpleScopedGuard<CustomMutex> lock(m_callbackMutex);
        (*callback)(info);
    }
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
    info.filePath = request.filePath;

    bool isDDS = false;
    if (request.filePath.size() > 4)
    {
        stltype::string extension = request.filePath.substr(request.filePath.size() - 4);
        if (extension == ".dds" || extension == ".DDS")
        {
            isDDS = true;
        }
    }

    if (isDDS)
    {
        tinyddsloader::DDSFile dds;
        auto rslt = dds.Load(request.filePath.data());
        if (rslt != tinyddsloader::Result::Success)
        {
            DEBUG_LOGF("[FileReader] Failed to load DDS: {}", request.filePath.data());
            return;
        }

        if (dds.GetMipCount() == 0)
        {
            DEBUG_LOGF("[FileReader] Empty mipmaps in DDS: {}", request.filePath.data());
            return;
        }

        info.extents.x = dds.GetWidth();
        info.extents.y = dds.GetHeight();
        info.ddsFormat = (u32)dds.GetFormat();
        info.mipmapPixels.reserve(dds.GetMipCount());
        u64 imageSize = 0;
        for (u32 i = 0; i < dds.GetMipCount(); ++i)
        {
            auto imageData = dds.GetImageData(i, 0);
            auto& mipData = info.mipmapPixels.emplace_back();
            mipData.size = imageData->m_memSlicePitch;
            mipData.pData = (unsigned char*)malloc(mipData.size);
            memcpy(mipData.pData, imageData->m_mem, mipData.size);
            imageSize = imageSize + mipData.size;
        }
        
        // Check if format has alpha. This is a bit simplified but usually works for common DDS formats.
        auto format = dds.GetFormat();
        info.supportsAlpha = true; // Most DXGI formats we care about support alpha, or we can check specifically if needed
        using DXGIFormat = tinyddsloader::DDSFile::DXGIFormat;
        if (format == DXGIFormat::BC4_SNorm || format == DXGIFormat::BC4_UNorm ||
            format == DXGIFormat::BC5_SNorm || format == DXGIFormat::BC5_UNorm ||
            format == DXGIFormat::R8_UNorm || format == DXGIFormat::R8G8_UNorm)
        {
            info.supportsAlpha = false;
        }

        info.dataSize = imageSize;
    }
    else
    {
        bool isHDR = false;
        if (request.filePath.size() > 4)
        {
            stltype::string extension = request.filePath.substr(request.filePath.size() - 4);
            if (extension == ".hdr" || extension == ".HDR")
            {
                isHDR = true;
            }
        }

        if (isHDR)
        {
            float* floatPixels = stbi_loadf(request.filePath.data(), &info.extents.x, &info.extents.y, &info.texChannels, STBI_rgb_alpha);
            if (!floatPixels && (request.filePath.rfind("Textures/", 0) == 0 || request.filePath.rfind("Textures\\", 0) == 0))
            {
                stltype::string fallbackPath = "../../" + request.filePath;
                floatPixels = stbi_loadf(fallbackPath.data(), &info.extents.x, &info.extents.y, &info.texChannels, STBI_rgb_alpha);
            }
            info.pixels = reinterpret_cast<unsigned char*>(floatPixels);
            info.supportsAlpha = true;
            if (!info.pixels)
            {
                DEBUG_LOG_ERRF("[FileReader] Failed to load HDR image: {}", request.filePath.c_str());
                return;
            }
            // Halved in place to RGBA16F, which is always filterable; halves never overtake the floats being read
            const u64 valueCount = (u64)info.extents.x * info.extents.y * 4;
            auto* pHalfs = reinterpret_cast<DirectX::PackedVector::HALF*>(floatPixels);
            for (u64 i = 0; i < valueCount; ++i)
                // The sun is brighter than fp16 can hold (+Inf turns into NaN in the shader), 65504 is the max finite half
                pHalfs[i] = DirectX::PackedVector::XMConvertFloatToHalf(stltype::min(floatPixels[i], 65504.0f));
            info.dataSize = valueCount * sizeof(DirectX::PackedVector::HALF);
            info.ddsFormat = static_cast<u32>(tinyddsloader::DDSFile::DXGIFormat::R16G16B16A16_Float);
        }
        else
        {
            info.pixels =
                stbi_load(request.filePath.data(), &info.extents.x, &info.extents.y, &info.texChannels, STBI_rgb_alpha);
            if (!info.pixels && (request.filePath.rfind("Textures/", 0) == 0 || request.filePath.rfind("Textures\\", 0) == 0))
            {
                stltype::string fallbackPath = "../../" + request.filePath;
                info.pixels = stbi_load(fallbackPath.data(), &info.extents.x, &info.extents.y, &info.texChannels, STBI_rgb_alpha);
            }
            info.dataSize = (u64)info.extents.x * info.extents.y * 4;
            info.ddsFormat = 0; // Standard RGBA8
            info.supportsAlpha = true;
            if (!info.pixels)
            {
                DEBUG_LOG_ERRF("[FileReader] Failed to load image: {}", request.filePath.c_str());
                return;
            }
        }
    }


    SimpleScopedGuard<CustomMutex> lock(m_completedMutex);
    m_completedImages.push_back({request, info});
}

u32 FileReader::GetPendingImageCount()
{
    SimpleScopedGuard<CustomMutex> lock(m_completedMutex);
    return (u32)m_completedImages.size();
}

void FileReader::FreeImageData(const unsigned char* pixels)
{
    // stbi_image_free usually just calls free, and we use malloc for DDS
    free((void*)pixels);
}

void FileReader::ReadMeshFile(const IORequest& request)
{
    ScopedZone("FileReader::Read Mesh File");

    Assimp::Importer importer;
    stltype::string_view path = request.filePath;
    const auto ext = path.substr(path.find_last_of('.'));
    DEBUG_ASSERT(importer.IsExtensionSupported(ext.data()));

    importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS,
        aiComponent_ANIMATIONS | aiComponent_BONEWEIGHTS | aiComponent_COLORS | aiComponent_TEXTURES);

    const aiScene* pMeshScene = importer.ReadFile(
        path.data(),
        aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_FlipUVs | aiProcess_RemoveComponent |
            aiProcess_RemoveRedundantMaterials | aiProcess_GenUVCoords | aiProcess_GenBoundingBoxes |
            aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace);

    DEBUG_ASSERT(pMeshScene);
    auto pScene = stltype::make_unique<DecodedScene>(MeshDecoder::Decode(pMeshScene));

    SimpleScopedGuard<CustomMutex> lock(m_completedMutex);
    m_completedMeshes.push_back({request, stltype::move(pScene)});
}
