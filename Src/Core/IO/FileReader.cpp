#include <fstream>
#include <filesystem>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h> 
#include <assimp/postprocess.h>

#include "Core/Global/GlobalVariables.h"
#include "FileReader.h"
#include "MeshConverter.h"

using namespace threadSTL;

FileReader::FileReader()
{
	m_ioThread = MakeThread([this]()
		{
			CheckIORequests();
		});
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
	for(int i = 0; i < m_requests.size(); ++i)
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
			threadSTL::ThreadSleep(500);
			continue;
		}
		m_requestSubmitMutex.Lock();
		const IORequest& request = m_requests.front();
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
		m_requestSubmitMutex.Lock();
		m_requests.pop();
		m_requestSubmitMutex.Unlock();
	}

}

void FileReader::ReadFileAsGenericBytes(const IORequest& request)
{
	ReadBytesInfo info{};
	info.bytes = ReadFileAsGenericBytes(request.filePath.data());

	const IOByteReadCallback* callback = stltype::get_if<IOByteReadCallback>(&request.callback);
	if(callback)
		(*callback)(info); 
}

stltype::vector<char> FileReader::ReadFileAsGenericBytes(const char* filePath)
{
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
	ReadTextureInfo info{};
	info.pixels = stbi_load(request.filePath.data(), &info.extents.x, &info.extents.y, &info.texChannels, STBI_rgb_alpha);
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
#define AI_CONFIG_PP_RVC_FLAGS aiComponent::aiComponent_COLORS | aiComponent::aiComponent_CAMERAS

	Assimp::Importer importer;
	stltype::string_view path = request.filePath;
	const auto ext = path.substr(path.find_last_of('.'));
	DEBUG_ASSERT(importer.IsExtensionSupported(ext.data()));
	const aiScene* pMeshScene = importer.ReadFile(path.data(), 
		aiProcess_Triangulate | 
		aiProcess_JoinIdenticalVertices |
		aiProcess_RemoveComponent |
		aiProcess_RemoveRedundantMaterials |
		aiProcess_GenUVCoords | 
		aiProcess_GenBoundingBoxes);

	DEBUG_ASSERT(pMeshScene);
	auto scene = MeshConversion::Convert(pMeshScene);

	const IOMeshReadCallback* callback = stltype::get_if<IOMeshReadCallback>(&request.callback);
	if(callback)
		(*callback)({scene});
}
