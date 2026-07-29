#pragma once
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "RGResourceRegistry.h"

namespace RenderPasses
{
struct FrameRendererContext;
struct MainPassData;
}

struct RGExecutionContext
{
    CommandBuffer* pCmdBuffer{nullptr};
    const RenderPasses::FrameRendererContext* pFrameCtx{nullptr};
    const stltype::vector<DescriptorSet::Ptr>* pResolvedDescriptors{nullptr};
    RGResourceRegistry* pRegistry{nullptr};

    const stltype::vector<DescriptorSet::Ptr>& GetDescriptors() const
    {
        DEBUG_ASSERT(pResolvedDescriptors != nullptr);
        return *pResolvedDescriptors;
    }

    Texture* GetTexture(RGResourceHandle handle) const
    {
        return pRegistry ? pRegistry->Resolve(handle) : nullptr;
    }

    Texture* GetTexture(RGResourceID id) const
    {
        return pRegistry ? pRegistry->ResolveByID(id) : nullptr;
    }

    Texture* GetHistoryTexture(RGResourceHandle handle) const
    {
        return pRegistry ? pRegistry->ResolveHistory(handle) : nullptr;
    }

    Texture* GetHistoryTexture(RGResourceID id) const
    {
        return pRegistry ? pRegistry->ResolveHistoryByID(id) : nullptr;
    }

    BindlessTextureHandle GetBindless(RGResourceHandle handle) const
    {
        return pRegistry ? pRegistry->ResolveBindless(handle) : 0;
    }

    BindlessTextureHandle GetBindless(RGResourceID id) const
    {
        return pRegistry ? pRegistry->ResolveBindlessByID(id) : 0;
    }

    BindlessTextureHandle GetHistoryBindless(RGResourceHandle handle) const
    {
        return pRegistry ? pRegistry->ResolveHistoryBindless(handle) : 0;
    }

    BindlessTextureHandle GetHistoryBindless(RGResourceID id) const
    {
        return pRegistry ? pRegistry->ResolveHistoryBindlessByID(id) : 0;
    }

    TextureHandle GetTextureHandle(RGResourceHandle handle) const
    {
        return pRegistry ? pRegistry->ResolveTextureHandle(handle) : 0;
    }

    TextureHandle GetTextureHandle(RGResourceID id) const
    {
        return pRegistry ? pRegistry->ResolveTextureHandleByID(id) : 0;
    }

    TextureHandle GetHistoryTextureHandle(RGResourceHandle handle) const
    {
        return pRegistry ? pRegistry->ResolveHistoryTextureHandle(handle) : 0;
    }

    TextureHandle GetHistoryTextureHandle(RGResourceID id) const
    {
        return pRegistry ? pRegistry->ResolveHistoryTextureHandleByID(id) : 0;
    }
};
