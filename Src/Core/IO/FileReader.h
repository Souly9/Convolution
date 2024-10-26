#pragma once
#include "Core/Global/GlobalDefines.h"

class FileReader
{
public:
	static stltype::vector<char> ReadFileAsGenericBytes(const stltype::string_view& filePath);
	static stltype::vector<char> ReadFileAsGenericBytes(const char* filePath);

	struct ReadTextureInfo
	{
		DirectX::XMINT2 extents;
		unsigned char* pixels;
		s32 texChannels;
	};
	static ReadTextureInfo ReadImageFile(const char* filePath);
	static void FreeImageData(unsigned char* pixels);
};