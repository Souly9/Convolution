#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Utils/MemoryUtilities.h"

// Pointer to the data
using Data = u64;
// Size of the data
using SizeType = size_t;
// Handle to the data, used by manager to keep track of data
using Handle = u64;

// Generic blob of memory allocated by memorymanager
struct MemoryBlob
{
	SizeType size;
	Data data;
	Handle handle;

	auto operator<=>(const MemoryBlob&) const = default;
};

HAS_NO_PADDING(MemoryBlob)