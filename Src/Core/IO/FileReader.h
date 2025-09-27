#pragma once
struct ReadTextureInfo;
#include "Core/Global/GlobalDefines.h"
#include <EASTL/fixed_function.h>
#include <EASTL/queue.h>
#include "Core/SceneGraph/Scene.h"

struct ReadTextureInfo
{
	stltype::string filePath;
	DirectX::XMINT2 extents;
	unsigned char* pixels;
	s32 texChannels;
};

struct ReadBytesInfo
{
	stltype::vector<char> bytes;
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
using IOByteReadCallback = stltype::fixed_function<4, void(const ReadBytesInfo&)>;
using IOMeshReadCallback = stltype::fixed_function<4, void(const ReadMeshInfo&)>;

using IOCallback = stltype::variant<IOImageReadCallback, IOByteReadCallback, IOMeshReadCallback>;

struct IORequest
{
	stltype::string filePath;
	IOCallback callback;
	RequestType requestType;
};

class FileReader
{
public:
	FileReader();
	~FileReader();

	// Stalls main thread through sleep until all requests are finished
	void FinishAllRequests();
	void CancelAllRequests();

	void SubmitIORequest(const IORequest& request);

	void CheckIORequests();

	static void FreeImageData(unsigned char* pixels);

	void ReadImageFile(const IORequest& request);
protected:

	void ReadFileAsGenericBytes(const IORequest& request);
	stltype::vector<char> ReadFileAsGenericBytes(const char* filePath);

	void ReadMeshFile(const IORequest& request);
	
	threadSTL::Thread m_ioThread;
	threadSTL::Mutex m_requestSubmitMutex{};
	stltype::queue<IORequest> m_requests{}; // Pending requests, read by iothread
	bool m_keepRunning{ true };
};