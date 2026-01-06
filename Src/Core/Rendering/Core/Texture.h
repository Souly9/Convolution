#pragma once
#include "Core/Global/EnumHelpers.h"
#include "Core/Rendering/LayerDefines.h"
#include "Resource.h"

enum class ImageLayout
{
    UNDEFINED,
    TRANSFER_SRC_OPTIMAL,
    TRANSFER_DST_OPTIMAL,
    SHADER_READ_OPTIMAL,
    COLOR_ATTACHMENT,
    DEPTH_STENCIL,
    PRESENT,
};

enum class LoadOp
{
    CLEAR,
    LOAD,
    IDC
};

enum class StoreOp
{
    STORE,
    IDC
};

enum class Tiling
{
    OPTIMAL,
    LINEAR
};

enum class Usage
{
    GBuffer,
    ColorAttachment,
    DepthAttachment,
    TransferSrc,
    TransferDst,
    Sampled,
    AttachmentReadWrite,
    StencilAttachment,
    ShadowMap
};
MAKE_FLAG_ENUM(Usage)

struct TextureInfoBase
{
    DirectX::XMUINT3 extents;
    ImageLayout layout{ImageLayout::UNDEFINED};
    u64 size{0};
    bool hasMipMaps{false};
};

struct TextureInfo : TextureInfoBase
{
    TexFormat format;
};

class Tex : public TrackedResource
{
public:
    Tex() = default;
    Tex(const TextureInfo& info) : m_info{info}
    {
    }

    virtual void CleanUp() override
    {
    }
    const TextureInfo& GetInfo() const
    {
        return m_info;
    }
    TextureInfo& GetInfo()
    {
        return m_info;
    }

protected:
    TextureInfo m_info;
};