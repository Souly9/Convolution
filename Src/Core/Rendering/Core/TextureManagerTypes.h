#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Typedefs.h"
#include "Core/IO/FileReader.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/Texture.h"

// API-agnostic texture manager request types, shared by VkTextureManager and MtlTextureManager

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
    bool hasMipMaps{false};
    bool isPersistent{false};
    u32 mipLevels;
    stltype::string name;
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
