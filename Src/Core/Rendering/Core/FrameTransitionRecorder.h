#pragma once
#include "Core/Rendering/Core/RenderTargetManager.h"

class FrameTransitionRecorder
{
public:
    void RecordTemporalResourceInitialLayouts(CommandBuffer* pCmdBuffer,
                                              GBuffer& gbuffer,
                                              Texture* pDLSSExposureTexture,
                                              StagingBuffer& dlssExposureStagingBuffer);
    void RecordInitialLayoutTransitions(CommandBuffer* pCmdBuffer,
                                        const stltype::fixed_vector<const Texture*, 16>& allGbufferAndSwapchain,
                                        const RenderPasses::RendererAttachmentInfo& attachments);
    void RecordPendingTextureUploadTransitions(CommandBuffer* pCmdBuffer);
    void RecordGBufferToShaderRead(CommandBuffer* pCmdBuffer,
                                   const stltype::fixed_vector<const Texture*, 8>& gbufferTextures,
                                   const RenderPasses::RendererAttachmentInfo& attachments);
    void RecordVelocityClear(CommandBuffer* pCmdBuffer, GBuffer& gbuffer);
    void RecordDepthToReadOnly(CommandBuffer* pCmdBuffer, const RenderPasses::RendererAttachmentInfo& attachments);
    void RecordThisFrameColorToRead(CommandBuffer* pCmdBuffer, GBuffer& gbuffer);
    void RecordTemporalCurrentColorToGeneral(CommandBuffer* pCmdBuffer, GBuffer& gbuffer);
    void RecordTemporalCurrentColorToRead(CommandBuffer* pCmdBuffer, GBuffer& gbuffer);
    void RecordResolveToGeneral(CommandBuffer* pCmdBuffer, GBuffer& gbuffer);
    void RecordResolveToRead(CommandBuffer* pCmdBuffer, GBuffer& gbuffer);
    void RecordCopyTextureToResolve(CommandBuffer* pCmdBuffer, GBuffer& gbuffer, Texture* pSourceTexture);
    void RecordClearColorTexture(CommandBuffer* pCmdBuffer,
                                 Texture* pTexture,
                                 ImageLayout oldLayout,
                                 ImageLayout finalLayout);
    void RecordSSSOutputToGeneral(CommandBuffer* pCmdBuffer, Texture* pScreenSpaceShadowTexture);
    void RecordSSSOutputToShaderRead(CommandBuffer* pCmdBuffer, Texture* pScreenSpaceShadowTexture);
    void RecordDLSSExposureUpdate(CommandBuffer* pCmdBuffer,
                                  Texture* pDLSSExposureTexture,
                                  StagingBuffer& dlssExposureStagingBuffer);
    void RecordSwapchainToPresent(CommandBuffer* pCmdBuffer, Texture* pSwapchainTexture);
};
