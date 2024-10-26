#pragma once
#include "Resource.h"

// Simple enum to manage memory flags at creation of buffer
// Assumes the application will not try to copy data into a GenericDeviceVisible buffer without staging buffer etc.
enum class BufferUsage
{
	Vertex,
	Index,
	Staging,
	Uniform,
	GenericHostVisible,
	GenericDeviceVisible,
	Texture
};
struct BufferCreateInfo
{
	u64 size;
	BufferUsage usage;
	bool isExclusive{ SEPERATE_TRANSFERQUEUE };
};

class GenBuffer : public TrackedResource
{

};

class VBuffer : public GenBuffer
{
public:

protected:
};
