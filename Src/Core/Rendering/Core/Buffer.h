#pragma once

#include "Resource.h"
#include "Core/Global/GlobalDefines.h"

// Simple enum to manage memory flags at creation of buffer
// Assumes the application will not try to copy data into a GenericDeviceVisible buffer without staging buffer etc.
enum class BufferUsage
{
    Vertex,
    Index,
    Staging,
    Uniform,
    SSBOHost,
    SSBODevice,
    IndirectDrawCmds,
    IndirectDrawCount,
    GenericHostVisible,
    GenericDeviceVisible,
    Texture
};

struct BufferCreateInfo
{
    u64 size;
    BufferUsage usage;
    bool isExclusive{SEPERATE_TRANSFERQUEUE};
};

class BufferBase : public TrackedResource
{
public:
    BufferBase() = default;
    virtual ~BufferBase() = default;
};

// Template wrapper for generic buffers
#include "APITraits.h"
#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VulkanTraits.h"
#include "Core/Rendering/Vulkan/VkBuffer.h"
#endif

template <typename API>
class GenBufferT : public APITraits<API>::BufferType
{
public:
    using APITraits<API>::BufferType::BufferType;
};
