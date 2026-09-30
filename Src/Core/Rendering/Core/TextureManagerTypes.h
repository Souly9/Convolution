#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Typedefs.h"
#include "Core/IO/FileReader.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/Texture.h"

// API-agnostic texture manager request types, shared by VkTextureManager and MtlTextureManager

struct TextureSamplerInfo
{
    TextureWrapMode wrapU{TextureWrapMode::REPEAT};
    TextureWrapMode wrapV{TextureWrapMode::REPEAT};
    TextureWrapMode wrapW{TextureWrapMode::REPEAT};
    TextureFilter minFilter{TextureFilter::LINEAR};
    TextureFilter magFilter{TextureFilter::LINEAR};
#ifdef USE_VULKAN
    VkBorderColor borderColor{VK_BORDER_COLOR_INT_OPAQUE_WHITE};
#endif
};

enum class TextureSemantic : u8
{
    Auto,
    BaseColor,
    Emissive,
    Normal,
    Data,
    Sheen,
    Clearcoat,
    Specular
};

struct DynamicTextureRequest
{
    DirectX::XMUINT3 extents;
    TextureHandle handle;
    TexFormat format;
    Usage usage;
    Tiling tiling{Tiling::OPTIMAL};
    TextureSamplerInfo samplerInfo;
    bool hasMipMaps{false};
    bool createSampler{true};
    bool isPersistent{false};
    u32 mipLevels;

    void AddName(const stltype::string& name)
    {
#ifdef CONV_DEBUG
        m_debugName = name;
#endif
    }

    const stltype::string& GetName() const
    {
#ifdef CONV_DEBUG
        return m_debugName;
#else
        return "DynamicTexture";
#endif
    }

private:
#ifdef CONV_DEBUG
    stltype::string m_debugName;
#endif
};

struct FileTextureRequest
{
    ReadTextureInfo ioInfo;
    TextureHandle handle;
    bool makeBindless{true};
    bool isPersistent{false};
    TextureSemantic semantic{TextureSemantic::Auto};
    TexFormat format{TexFormat::UNDEFINED};
};
struct AsyncLayoutTransitionRequest
{
    stltype::vector<const Texture*> textures;
    ImageLayout oldLayout;
    ImageLayout newLayout;
    u32 mipLevels;
    // Optional semaphores to wait on and signal
    Semaphore* pWaitSemaphore{nullptr};
    Semaphore* pSignalSemaphore{nullptr};

    // Timeline semaphores
    TimelineSemaphore* pTimelineWaitSemaphore{nullptr};
    u64 timelineWaitValue{0};
    TimelineSemaphore* pTimelineSignalSemaphore{nullptr};
    u64 timelineSignalValue{0};
};

using TextureRequest = stltype::variant<FileTextureRequest, DynamicTextureRequest, AsyncLayoutTransitionRequest>;

struct TextureFileCreateInfo
{
    stltype::string filePath;
    bool makeBindless{true};
    bool isPersistent{false};
    TextureSemantic semantic{TextureSemantic::Auto};

    TextureFileCreateInfo(const stltype::string& filePath, bool makeBindless, TextureSemantic semantic, bool isPersistent = false)
        : filePath(filePath), makeBindless(makeBindless), isPersistent(isPersistent), semantic(semantic)
    {
    }
};

struct LoadedTextureInfo
{
    stltype::string filePath;
    TextureSemantic semantic{TextureSemantic::Auto};
    TextureHandle handle;
};
