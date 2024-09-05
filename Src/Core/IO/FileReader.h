#pragma once
#include "Core/Global/GlobalDefines.h"

class FileReader
{
public:
	static stltype::vector<char> ReadFileAsGenericBytes(const stltype::string_view& filePath);
	static stltype::vector<char> ReadFileAsGenericBytes(const char* filePath);
};