#include "FrameTransitionRecorder.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"
#include "Core/Global/State/ApplicationState.h"
#include "vulkan/vulkan_core.h"

void FrameTransitionRecorder::RecordTemporalResourceInitialLayouts(
    CommandBuffer* pCmdBuffer,
    GBuffer& gbuffer,
    Texture* pDLSSExposureTexture,
    StagingBuffer& dlssExposureStagingBuffer)
{
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        Texture* pTexture = gbuffer.GetTemporalResolveTexture(i);
        if (pTexture != nullptr)
        {
            RecordClearColorTexture(
                pCmdBuffer, pTexture, ImageLayout::UNDEFINED, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        }
    }

    Texture* pPostAA = gbuffer.Get(GBufferTextureType::GBufferPostAAColor);
    if (pPostAA)
    {
        RecordClearColorTexture(
            pCmdBuffer, pPostAA, ImageLayout::UNDEFINED, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    }

    if (pDLSSExposureTexture)
    {
        const float exposureValue = g_pApplicationState->GetCurrentApplicationState().renderState.exposure;
        dlssExposureStagingBuffer.CopyToMapped(&exposureValue, sizeof(exposureValue));

        ImageLayoutTransitionCmd exposureToTransfer(pDLSSExposureTexture);
        exposureToTransfer.oldLayout = ImageLayout::UNDEFINED;
        exposureToTransfer.newLayout = ImageLayout::TRANSFER_DST_OPTIMAL;
        VkTextureManager::SetLayoutBarrierMasks(
            exposureToTransfer, ImageLayout::UNDEFINED, ImageLayout::TRANSFER_DST_OPTIMAL);
        pCmdBuffer->RecordCommand(exposureToTransfer);

        ImageBufferCopyCmd copyExposure(&dlssExposureStagingBuffer, pDLSSExposureTexture);
        copyExposure.imageExtent = {1, 1, 1};
        copyExposure.aspectFlagBits = VK_IMAGE_ASPECT_COLOR_BIT;
        pCmdBuffer->RecordCommand(copyExposure);

        ImageLayoutTransitionCmd exposureToRead(pDLSSExposureTexture);
        exposureToRead.oldLayout = ImageLayout::TRANSFER_DST_OPTIMAL;
        exposureToRead.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
        VkTextureManager::SetLayoutBarrierMasks(
            exposureToRead, ImageLayout::TRANSFER_DST_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        exposureToRead.dstStage = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        pCmdBuffer->RecordCommand(exposureToRead);
    }
}

void FrameTransitionRecorder::RecordInitialLayoutTransitions(
    CommandBuffer* pCmdBuffer,
    const stltype::fixed_vector<const Texture*, 16>& allGbufferAndSwapchain,
    const RenderPasses::RendererAttachmentInfo& attachments)
{
    stltype::vector<const Texture*> colorTextures;
    colorTextures.reserve(allGbufferAndSwapchain.size());
    for (const auto* pTexture : allGbufferAndSwapchain)
    {
        if (pTexture != nullptr)
            colorTextures.push_back(pTexture);
    }
    if (!colorTextures.empty())
    {
        ImageLayoutTransitionCmd colorCmd(colorTextures);
        colorCmd.oldLayout = ImageLayout::UNDEFINED;
        colorCmd.newLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
        VkTextureManager::SetLayoutBarrierMasks(colorCmd, ImageLayout::UNDEFINED, ImageLayout::COLOR_ATTACHMENT_OPTIMAL);
        pCmdBuffer->RecordCommand(colorCmd);
    }

    stltype::fixed_vector<const Texture*, 2> depthTextures;
    if (attachments.depthAttachment.GetTexture() != nullptr)
        depthTextures.push_back(attachments.depthAttachment.GetTexture());
    if (attachments.directionalLightShadowMap.pTexture)
        depthTextures.push_back(attachments.directionalLightShadowMap.pTexture);

    if (!depthTextures.empty())
    {
        ImageLayoutTransitionCmd depthCmd(stltype::vector<const Texture*>(depthTextures.begin(), depthTextures.end()));
        depthCmd.oldLayout = ImageLayout::UNDEFINED;
        depthCmd.newLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkTextureManager::SetLayoutBarrierMasks(
            depthCmd, ImageLayout::UNDEFINED, ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        pCmdBuffer->RecordCommand(depthCmd);
    }
}

void FrameTransitionRecorder::RecordPendingTextureUploadTransitions(CommandBuffer* pCmdBuffer)
{
    const auto pendingTextures = g_pTexManager->PopPendingGraphicsShaderReadTransitions();
    if (pendingTextures.empty())
        return;

    stltype::vector<const Texture*> textures;
    textures.reserve(pendingTextures.size());
    for (const auto* pTexture : pendingTextures)
        textures.push_back(pTexture);

    ImageLayoutTransitionCmd cmd(textures);
    cmd.oldLayout = ImageLayout::TRANSFER_DST_OPTIMAL;
    cmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::TRANSFER_DST_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(cmd);
}

void FrameTransitionRecorder::RecordGBufferToShaderRead(
    CommandBuffer* pCmdBuffer,
    const stltype::fixed_vector<const Texture*, 8>& gbufferTextures,
    const RenderPasses::RendererAttachmentInfo& attachments)
{
    ImageLayoutTransitionCmd colorCmd(stltype::vector<const Texture*>(gbufferTextures.begin(), gbufferTextures.end()));
    colorCmd.oldLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    colorCmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(
        colorCmd, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(colorCmd);

    if (const auto* pShadowMap = attachments.directionalLightShadowMap.pTexture)
    {
        ImageLayoutTransitionCmd shadowCmd(pShadowMap);
        shadowCmd.oldLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        shadowCmd.newLayout = ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        VkTextureManager::SetLayoutBarrierMasks(
            shadowCmd, ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        pCmdBuffer->RecordCommand(shadowCmd);
    }
}

void FrameTransitionRecorder::RecordVelocityClear(CommandBuffer* pCmdBuffer, GBuffer& gbuffer)
{
    RecordClearColorTexture(pCmdBuffer,
                            gbuffer.Get(GBufferTextureType::GBufferVelocity),
                            ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
                            ImageLayout::COLOR_ATTACHMENT_OPTIMAL);
}

void FrameTransitionRecorder::RecordDepthToReadOnly(CommandBuffer* pCmdBuffer,
                                                    const RenderPasses::RendererAttachmentInfo& attachments)
{
    ImageLayoutTransitionCmd depthCmd(attachments.depthAttachment.GetTexture());
    depthCmd.oldLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthCmd.newLayout = ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(
        depthCmd, ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(depthCmd);
}

void FrameTransitionRecorder::RecordThisFrameColorToRead(CommandBuffer* pCmdBuffer, GBuffer& gbuffer)
{
    ImageLayoutTransitionCmd cmd(gbuffer.Get(GBufferTextureType::GBufferThisFrameColor));
    cmd.oldLayout = ImageLayout::GENERAL;
    cmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::GENERAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(cmd);
}

void FrameTransitionRecorder::RecordThisFrameColorToGeneral(CommandBuffer* pCmdBuffer, GBuffer& gbuffer)
{
    ImageLayoutTransitionCmd cmd(gbuffer.Get(GBufferTextureType::GBufferThisFrameColor));
    cmd.oldLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    cmd.newLayout = ImageLayout::GENERAL;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::SHADER_READ_ONLY_OPTIMAL, ImageLayout::GENERAL);
    pCmdBuffer->RecordCommand(cmd);
}

void FrameTransitionRecorder::RecordThisFrameColorToGeneralDiscard(CommandBuffer* pCmdBuffer, GBuffer& gbuffer)
{
    ImageLayoutTransitionCmd cmd(gbuffer.Get(GBufferTextureType::GBufferThisFrameColor));
    cmd.oldLayout = ImageLayout::UNDEFINED;
    cmd.newLayout = ImageLayout::GENERAL;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::UNDEFINED, ImageLayout::GENERAL);
    pCmdBuffer->RecordCommand(cmd);
}

void FrameTransitionRecorder::RecordThisFrameColorFromGeneralToRead(CommandBuffer* pCmdBuffer, GBuffer& gbuffer)
{
    ImageLayoutTransitionCmd cmd(gbuffer.Get(GBufferTextureType::GBufferThisFrameColor));
    cmd.oldLayout = ImageLayout::GENERAL;
    cmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::GENERAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(cmd);
}

void FrameTransitionRecorder::RecordResolveToGeneral(CommandBuffer* pCmdBuffer, GBuffer& gbuffer)
{
    Texture* pResolve = gbuffer.Get(GBufferTextureType::GBufferResolve);
    ImageLayoutTransitionCmd cmd(pResolve);
    cmd.oldLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    cmd.newLayout = ImageLayout::GENERAL;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::SHADER_READ_ONLY_OPTIMAL, ImageLayout::GENERAL);
    pCmdBuffer->RecordCommand(cmd);
}

void FrameTransitionRecorder::RecordResolveToRead(CommandBuffer* pCmdBuffer, GBuffer& gbuffer)
{
    ImageLayoutTransitionCmd cmd(gbuffer.Get(GBufferTextureType::GBufferResolve));
    cmd.oldLayout = ImageLayout::GENERAL;
    cmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::GENERAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(cmd);
}

void FrameTransitionRecorder::RecordCopyTextureToResolve(CommandBuffer* pCmdBuffer,
                                                         GBuffer& gbuffer,
                                                         Texture* pSourceTexture)
{
    Texture* pResolve = gbuffer.Get(GBufferTextureType::GBufferResolve);

    ImageLayoutTransitionCmd sourceToCopy(pSourceTexture);
    sourceToCopy.oldLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    sourceToCopy.newLayout = ImageLayout::TRANSFER_SRC_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(
        sourceToCopy, ImageLayout::SHADER_READ_ONLY_OPTIMAL, ImageLayout::TRANSFER_SRC_OPTIMAL);
    pCmdBuffer->RecordCommand(sourceToCopy);

    ImageLayoutTransitionCmd resolveToCopy(pResolve);
    resolveToCopy.oldLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    resolveToCopy.newLayout = ImageLayout::TRANSFER_DST_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(resolveToCopy, ImageLayout::SHADER_READ_ONLY_OPTIMAL, ImageLayout::TRANSFER_DST_OPTIMAL);
    pCmdBuffer->RecordCommand(resolveToCopy);

    const auto sourceExtents = pSourceTexture->GetInfo().extents;
    const auto resolveExtents = pResolve->GetInfo().extents;
    if (sourceExtents.x == resolveExtents.x && sourceExtents.y == resolveExtents.y)
        pCmdBuffer->RecordCommand(ImageToImageCopyCmd(pSourceTexture, pResolve));
    else
        pCmdBuffer->RecordCommand(ImageToImageBlitCmd(pSourceTexture, pResolve));

    ImageLayoutTransitionCmd sourceToRead(pSourceTexture);
    sourceToRead.oldLayout = ImageLayout::TRANSFER_SRC_OPTIMAL;
    sourceToRead.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(
        sourceToRead, ImageLayout::TRANSFER_SRC_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(sourceToRead);

    ImageLayoutTransitionCmd resolveToRead(pResolve);
    resolveToRead.oldLayout = ImageLayout::TRANSFER_DST_OPTIMAL;
    resolveToRead.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(
        resolveToRead, ImageLayout::TRANSFER_DST_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(resolveToRead);
}

void FrameTransitionRecorder::RecordClearColorTexture(CommandBuffer* pCmdBuffer,
                                                      Texture* pTexture,
                                                      ImageLayout oldLayout,
                                                      ImageLayout finalLayout)
{
    ImageLayoutTransitionCmd toTransfer(pTexture);
    toTransfer.oldLayout = oldLayout;
    toTransfer.newLayout = ImageLayout::TRANSFER_DST_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(toTransfer, oldLayout, ImageLayout::TRANSFER_DST_OPTIMAL);
    pCmdBuffer->RecordCommand(toTransfer);

    ClearColorImageCmd clearCmd(pTexture);
    clearCmd.color.float32[0] = 0.0f;
    clearCmd.color.float32[1] = 0.0f;
    clearCmd.color.float32[2] = 0.0f;
    clearCmd.color.float32[3] = 0.0f;
    pCmdBuffer->RecordCommand(clearCmd);

    ImageLayoutTransitionCmd toFinal(pTexture);
    toFinal.oldLayout = ImageLayout::TRANSFER_DST_OPTIMAL;
    toFinal.newLayout = finalLayout;
    VkTextureManager::SetLayoutBarrierMasks(toFinal, ImageLayout::TRANSFER_DST_OPTIMAL, finalLayout);
    pCmdBuffer->RecordCommand(toFinal);
}

void FrameTransitionRecorder::RecordSSSOutputToGeneral(CommandBuffer* pCmdBuffer, Texture* pScreenSpaceShadowTexture)
{
    if (!pScreenSpaceShadowTexture)
        return;
    ImageLayoutTransitionCmd cmd(pScreenSpaceShadowTexture);
    cmd.oldLayout = ImageLayout::UNDEFINED;
    cmd.newLayout = ImageLayout::GENERAL;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::UNDEFINED, ImageLayout::GENERAL);
    pCmdBuffer->RecordCommand(cmd);
}

void FrameTransitionRecorder::RecordSSSOutputToShaderRead(CommandBuffer* pCmdBuffer, Texture* pScreenSpaceShadowTexture)
{
    if (!pScreenSpaceShadowTexture)
        return;
    ImageLayoutTransitionCmd cmd(pScreenSpaceShadowTexture);
    cmd.oldLayout = ImageLayout::GENERAL;
    cmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::GENERAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    cmd.dstStage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    cmd.dstAccessMask = 0;
    pCmdBuffer->RecordCommand(cmd);
}

void FrameTransitionRecorder::RecordDLSSExposureUpdate(CommandBuffer* pCmdBuffer,
                                                       Texture* pDLSSExposureTexture,
                                                       StagingBuffer& dlssExposureStagingBuffer)
{
    if (!pDLSSExposureTexture)
        return;

    const float exposureValue = g_pApplicationState->GetCurrentApplicationState().renderState.exposure;
    dlssExposureStagingBuffer.CopyToMapped(&exposureValue, sizeof(exposureValue));

    ImageLayoutTransitionCmd exposureToTransfer(pDLSSExposureTexture);
    exposureToTransfer.oldLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    exposureToTransfer.newLayout = ImageLayout::TRANSFER_DST_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(
        exposureToTransfer, ImageLayout::SHADER_READ_ONLY_OPTIMAL, ImageLayout::TRANSFER_DST_OPTIMAL);
    exposureToTransfer.srcStage = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    pCmdBuffer->RecordCommand(exposureToTransfer);

    ImageBufferCopyCmd copyExposure(&dlssExposureStagingBuffer, pDLSSExposureTexture);
    copyExposure.imageExtent = {1, 1, 1};
    copyExposure.aspectFlagBits = VK_IMAGE_ASPECT_COLOR_BIT;
    pCmdBuffer->RecordCommand(copyExposure);

    ImageLayoutTransitionCmd exposureToRead(pDLSSExposureTexture);
    exposureToRead.oldLayout = ImageLayout::TRANSFER_DST_OPTIMAL;
    exposureToRead.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(
        exposureToRead, ImageLayout::TRANSFER_DST_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    exposureToRead.dstStage = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    pCmdBuffer->RecordCommand(exposureToRead);
}

void FrameTransitionRecorder::RecordSwapchainToPresent(CommandBuffer* pCmdBuffer, Texture* pSwapchainTexture)
{
    ImageLayoutTransitionCmd cmd(pSwapchainTexture);
    cmd.oldLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    cmd.newLayout = ImageLayout::PRESENT_SRC_KHR;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, ImageLayout::PRESENT_SRC_KHR);
    pCmdBuffer->RecordCommand(cmd);
}
