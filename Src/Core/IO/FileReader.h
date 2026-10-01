#pragma once
struct ReadTextureInfo;
struct SceneNode;
struct DecodedScene;
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/ThreadBase.h"
#include "Core/Global/ThreadPool.h"
#include "Core/SceneGraph/Scene.h"
#include <EASTL/deque.h>
#include <EASTL/fixed_function.h>

struct ReadMipmapInfo
{
    unsigned char* pData;
    u64 size;
};

struct ReadTextureInfo
{
    stltype::string filePath;
    DirectX::XMINT2 extents;
    unsigned char* pixels;
    stltype::vector<ReadMipmapInfo> mipmapPixels;
    s32 texChannels;
    u64 dataSize = 0;
    u32 ddsFormat = 0;
    bool supportsAlpha = false;
    bool autoFree = true;
};

struct ReadBytesInfo
{
    stltype::vector<char> bytes;
    stltype::string filePath;
};

struct ReadMeshInfo
{
    SceneNode rootNode;
};

enum class RequestType
{
    Bytes,
    Image,
    Mesh
};

using IOImageReadCallback = stltype::fixed_function<20, void(const ReadTextureInfo&)>;
using IOByteReadCallback = stltype::fixed_function<64, void(ReadBytesInfo&)>;
using IOMeshReadCallback = stltype::fixed_function<4, void(const ReadMeshInfo&)>;

using IOCallback = stltype::variant<IOImageReadCallback, IOByteReadCallback, IOMeshReadCallback>;

struct IORequest
{
    stltype::string filePath;
    IOCallback callback;
    RequestType requestType;
    // Stamped at submit, results of an older generation are dropped at delivery
    u32 generation{0};
    // Survives BumpGeneration, for textures that outlive a scene
    bool isPersistent{false};
};

// Image and mesh results are queued and run through their callbacks by DeliverCompleted on the caller's thread
class FileReader
{
public:
    FileReader();
    ~FileReader();

    // Blocks until the pool is idle
    void FinishAllRequests();
    // Joins the pool and frees results nobody picked up
    void Stop();

    void SubmitIORequest(const IORequest& request);

    // Runs the callbacks of finished requests, all meshes but at most maxImages images
    void DeliverCompleted(u32 maxImages);
    // Results of requests submitted before this call are dropped at delivery
    void BumpGeneration()
    {
        ++m_generation;
    }

    // Decoded images that wait for their frame
    u32 GetPendingImageCount();
    u32 GetImagesDeliveredLastCall() const
    {
        return m_lastDeliveredImages;
    }

    static void FreeImageData(const unsigned char* pixels);
    // Frees the pixels and every mip
    static void FreeTextureInfo(const ReadTextureInfo& info);

protected:
    void ReadImageFile(const IORequest& request);
    struct CompletedImage
    {
        IORequest request;
        ReadTextureInfo info;
    };
    struct CompletedMesh
    {
        IORequest request;
        stltype::unique_ptr<DecodedScene> pScene;
    };

    void ReadFileAsGenericBytes(const IORequest& request);
    stltype::vector<char> ReadFileAsGenericBytes(const char* filePath);

    void ReadMeshFile(const IORequest& request);
    bool IsStale(const IORequest& request) const;

    // Byte callbacks fill shared shader containers, so they run one at a time
    CustomMutex m_callbackMutex{};
    CustomMutex m_completedMutex{};
    stltype::deque<CompletedImage> m_completedImages;
    stltype::deque<CompletedMesh> m_completedMeshes;
    stltype::atomic<u32> m_generation{0};
    u32 m_lastDeliveredImages{0};
    // Last member: its destructor drains the queue into the members above
    ThreadPool m_threadPool;
};