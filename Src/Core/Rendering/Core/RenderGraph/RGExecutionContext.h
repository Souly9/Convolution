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

    u64 GetFrameCounter() const
    {
        return pFrameCtx ? pFrameCtx->frameCounter : 0;
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

    bool HasReadyTLAS() const
    {
        return pMainPassData && pMainPassData->pRTSceneManager && pFrameCtx &&
               pMainPassData->pRTSceneManager->HasReadyTLAS(pFrameCtx->currentFrame);
    }

    Texture* GetTexture(RGResourceID id) const
    {
        return pRegistry ? pRegistry->ResolveByID(id) : nullptr;
    }

    RenderAttachmentInfo GetColorAttachment(RGResourceID id, LoadOp loadOp = LoadOp::CLEAR, StoreOp storeOp = StoreOp::STORE, ImageLayout layout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL) const
    {
        return pRegistry ? pRegistry->GetColorAttachment(id, loadOp, storeOp, layout) : RenderAttachmentInfo{};
    }

    RenderAttachmentInfo GetDepthAttachment(RGResourceID id, LoadOp loadOp = LoadOp::CLEAR, StoreOp storeOp = StoreOp::STORE, ImageLayout layout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL) const
    {
        return pRegistry ? pRegistry->GetDepthAttachment(id, loadOp, storeOp, layout) : RenderAttachmentInfo{};
    }

    RenderAttachmentInfo GetReadOnlyDepthAttachment(RGResourceID id = RGResourceID::MainDepth, LoadOp loadOp = LoadOp::LOAD) const
    {
        return pRegistry ? pRegistry->GetReadOnlyDepthAttachment(id, loadOp) : RenderAttachmentInfo{};
    }

    BindlessTextureHandle GetBindless(RGResourceID id) const
    {
        return pRegistry ? pRegistry->ResolveBindlessByID(id) : 0;
    }

    BindlessTextureHandle GetHistoryBindless(RGResourceID id) const
    {
        return pRegistry ? pRegistry->ResolveHistoryBindlessByID(id) : 0;
    }
};
