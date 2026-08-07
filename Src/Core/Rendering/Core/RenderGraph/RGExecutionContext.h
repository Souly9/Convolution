#pragma once
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/FrameResourceManager.h"
#include "Core/Rendering/Core/RT/RTSceneManager.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Passes/MainPassData.h"
#include "RGResourceRegistry.h"

struct RGExecutionContext
{
    CommandBuffer* pCmdBuffer{nullptr};
    const RenderPasses::FrameRendererContext* pFrameCtx{nullptr};
    const RenderPasses::MainPassData* pMainPassData{nullptr};
    const stltype::vector<DescriptorSet::Ptr>* pResolvedDescriptors{nullptr};
    RGResourceRegistry* pRegistry{nullptr};

    const stltype::vector<DescriptorSet::Ptr>& GetDescriptors() const
    {
        DEBUG_ASSERT(pResolvedDescriptors != nullptr);
        return *pResolvedDescriptors;
    }

    u32 GetFrameIndex() const
    {
        return pFrameCtx ? pFrameCtx->currentFrame : 0;
    }

    f32 GetZNear() const
    {
        return pFrameCtx ? pFrameCtx->zNear : 0.1f;
    }

    f32 GetZFar() const
    {
        return pFrameCtx ? pFrameCtx->zFar : 300.0f;
    }

    u32 GetNumLights() const
    {
        return pFrameCtx ? pFrameCtx->numLights : 0;
    }

    mathstl::Vector2 GetRenderResolution() const
    {
        return pMainPassData ? pMainPassData->renderState.renderResolution : mathstl::Vector2{0.0f, 0.0f};
    }

    mathstl::Vector2 GetSwapchainResolution() const
    {
        return pMainPassData ? pMainPassData->renderState.swapchainResolution : mathstl::Vector2{0.0f, 0.0f};
    }

    bool HasReadyTLAS() const
    {
        return pMainPassData && pMainPassData->pRTSceneManager && pFrameCtx &&
               pMainPassData->pRTSceneManager->HasReadyTLAS(pFrameCtx->currentFrame);
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
