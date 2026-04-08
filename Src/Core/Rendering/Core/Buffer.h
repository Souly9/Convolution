#pragma once
#include "RenderTraitsMacros.h"

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

class StagingBufferBase : public BufferBase
{
public:
    StagingBufferBase() = default;
    virtual ~StagingBufferBase() = default;
};

class UniformBufferBase : public BufferBase
{
public:
    UniformBufferBase() = default;
    virtual ~UniformBufferBase() = default;
};

class StorageBufferBase : public BufferBase
{
public:
    StorageBufferBase() = default;
    virtual ~StorageBufferBase() = default;
};

class IndirectDrawCommandBufferBase : public BufferBase
{
public:
    IndirectDrawCommandBufferBase() = default;
    virtual ~IndirectDrawCommandBufferBase() = default;
};

class VertexBufferBase : public BufferBase
{
public:
    VertexBufferBase() = default;
    virtual ~VertexBufferBase() = default;
};

class IndexBufferBase : public BufferBase
{
public:
    IndexBufferBase() = default;
    virtual ~IndexBufferBase() = default;
};

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
    DECLARE_RENDER_RESOURCE_TRAITS(GenBufferT, BufferType)
};

template <typename API>
class StagingBufferT : public APITraits<API>::StagingBufferType
{
public:
    using APITraits<API>::StagingBufferType::StagingBufferType;
    DECLARE_RENDER_RESOURCE_TRAITS(StagingBufferT, StagingBufferType)
};

template <typename API>
class UniformBufferT : public APITraits<API>::UniformBufferType
{
public:
    using APITraits<API>::UniformBufferType::UniformBufferType;
    DECLARE_RENDER_RESOURCE_TRAITS(UniformBufferT, UniformBufferType)
};

template <typename API>
class StorageBufferT : public APITraits<API>::StorageBufferType
{
public:
    using APITraits<API>::StorageBufferType::StorageBufferType;
    DECLARE_RENDER_RESOURCE_TRAITS(StorageBufferT, StorageBufferType)
};

template <typename API>
class IndirectDrawCommandBufferT : public APITraits<API>::IndirectDrawCommandBufferType
{
public:
    using APITraits<API>::IndirectDrawCommandBufferType::IndirectDrawCommandBufferType;
    DECLARE_RENDER_RESOURCE_TRAITS(IndirectDrawCommandBufferT, IndirectDrawCommandBufferType)
};

template <typename API>
class VertexBufferT : public APITraits<API>::VertexBufferType
{
public:
    using APITraits<API>::VertexBufferType::VertexBufferType;
    DECLARE_RENDER_RESOURCE_TRAITS(VertexBufferT, VertexBufferType)
};

template <typename API>
class IndexBufferT : public APITraits<API>::IndexBufferType
{
public:
    using APITraits<API>::IndexBufferType::IndexBufferType;
    DECLARE_RENDER_RESOURCE_TRAITS(IndexBufferT, IndexBufferType)
};

