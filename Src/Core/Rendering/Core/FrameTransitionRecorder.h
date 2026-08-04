#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include <EASTL/fixed_vector.h>

class RGResourceRegistry;

class FrameTransitionRecorder
{
public:
    void RecordTemporalResourceInitialLayouts(CommandBuffer* pCmdBuffer,
                                              RGResourceRegistry& registry,
                                              StagingBuffer& dlssExposureStagingBuffer);
    void RecordInitialLayoutTransitions(CommandBuffer* pCmdBuffer,
                                        const stltype::fixed_vector<const Texture*, 16>& allGbufferAndSwapchain,
                                        Texture* pMainDepthTexture,
                                        Texture* pShadowMapTexture);
    void RecordPendingTextureUploadTransitions(CommandBuffer* pCmdBuffer);
    void RecordGBufferToShaderRead(CommandBuffer* pCmdBuffer,
                                   const stltype::fixed_vector<const Texture*, 8>& gbufferTextures,
                                   Texture* pShadowMapTexture);
    void RecordVelocityClear(CommandBuffer* pCmdBuffer, Texture* pVelocityTexture);
    void RecordDepthToReadOnly(CommandBuffer* pCmdBuffer, Texture* pMainDepthTexture);
    void RecordThisFrameColorToRead(CommandBuffer* pCmdBuffer, Texture* pThisFrameColorTexture);
    void RecordThisFrameColorToGeneral(CommandBuffer* pCmdBuffer, Texture* pThisFrameColorTexture);
    void RecordThisFrameColorToGeneralDiscard(CommandBuffer* pCmdBuffer, Texture* pThisFrameColorTexture);
    void RecordThisFrameColorFromGeneralToRead(CommandBuffer* pCmdBuffer, Texture* pThisFrameColorTexture);
    void RecordResolveToGeneral(CommandBuffer* pCmdBuffer, Texture* pResolveTexture);
    void RecordResolveToRead(CommandBuffer* pCmdBuffer, Texture* pResolveTexture);
    void RecordCopyTextureToResolve(CommandBuffer* pCmdBuffer, Texture* pResolveTexture, Texture* pSourceTexture);
    static void RecordClearColorTexture(CommandBuffer* pCmdBuffer,
                                        Texture* pTexture,
                                        ImageLayout oldLayout,
                                        ImageLayout finalLayout);
    void RecordSSSOutputToGeneral(CommandBuffer* pCmdBuffer, Texture* pScreenSpaceShadowTexture);
    void RecordSSSOutputToShaderRead(CommandBuffer* pCmdBuffer, Texture* pScreenSpaceShadowTexture);
    void RecordDLSSExposureUpdate(CommandBuffer* pCmdBuffer,
                                  Texture* pDLSSExposureTexture,
                                  StagingBuffer& dlssExposureStagingBuffer);
    void RecordSwapchainToAttachment(CommandBuffer* pCmdBuffer, Texture* pSwapchainTexture);
    void RecordSwapchainToPresent(CommandBuffer* pCmdBuffer, Texture* pSwapchainTexture);
};
