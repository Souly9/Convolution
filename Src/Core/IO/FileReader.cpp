#include <fstream>
#include <filesystem>
#include "FileReader.h"

stltype::vector<char> FileReader::ReadFileAsGenericBytes(const stltype::string_view& filePath)
{
	return ReadFileAsGenericBytes(filePath.data());
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
