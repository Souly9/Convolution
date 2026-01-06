#pragma once
#include "Core/Global/EnumHelpers.h"
#include "Core/Global/GlobalDefines.h"

// Class defining memory barrier enums to synchronize resource access

enum class MemoryBarrierType
{
    Image
};

enum class StageFlags
{
    VertexShader = 1 << 0,
    FragmentShader = 1 << 1,
    ComputeShader = 1 << 2,
    AllGraphics = VertexShader | FragmentShader,
    AllCompute = ComputeShader
};

enum class AccessFlags : u32
{
    VertexBufferRead = 1 << 0,
    IndexBufferRead = 1 << 1,
    UniformBufferRead = 1 << 2,
    StorageBufferRead = 1 << 3,
    TextureRead = 1 << 4,
    TextureWrite = 1 << 5,
    ShaderWrite = 1 << 6,
    TransferRead = 1 << 7,
    TransferWrite = 1 << 8,
};
MAKE_FLAG_ENUM(AccessFlags)

enum class AspectFlags : u32
{
    Color = 1 << 0,
    Depth = 1 << 1,
    Stencil = 1 << 2,
    DepthStencil = Depth | Stencil,
};
MAKE_FLAG_ENUM(AspectFlags)

enum class MemoryBarrierTemplate
{
    ColorAttachmentWriteNoRead,
    ColorAttachmentReadWrite,
};

struct ImageMemoryBarrier
{
    MemoryBarrierType type;
    StageFlags srcStage;
    StageFlags dstStage;
    AccessFlags srcAccessMask;
    AccessFlags dstAccessMask;
    AspectFlags aspectMask;
    u32 baseMipLevel;
    u32 levelCount;
    u32 baseArrayLayer;
    u32 layerCount;

    ImageMemoryBarrier(MemoryBarrierType t,
                       StageFlags src,
                       StageFlags dst,
                       AccessFlags srcAccess,
                       AccessFlags dstAccess,
                       AspectFlags aspect,
                       u32 baseMip,
                       u32 levelCnt,
                       u32 baseLayer,
                       u32 layerCnt)
        : type(t), srcStage(src), dstStage(dst), srcAccessMask(srcAccess), dstAccessMask(dstAccess), aspectMask(aspect),
          baseMipLevel(baseMip), levelCount(levelCnt), baseArrayLayer(baseLayer), layerCount(layerCnt)
    {
    }

    template <MemoryBarrierTemplate memoryTemplate>
    static ImageMemoryBarrier FromTemplate(MemoryBarrierTemplate barrierTemplate)
    {
        // Specialize for each MemoryBarrierTemplate type
        DEBUG_ASSERT(false);
    }

    template <>
    static ImageMemoryBarrier FromTemplate<MemoryBarrierTemplate::ColorAttachmentReadWrite>(
        MemoryBarrierTemplate barrierTemplate);
};
