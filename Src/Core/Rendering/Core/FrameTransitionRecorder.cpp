#include "FrameTransitionRecorder.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/Core/RenderGraph/RGResourceRegistry.h"
#include "Core/Global/State/ApplicationState.h"
#ifdef USE_VULKAN
#include "vulkan/vulkan_core.h"
#else
// Metal ignores stage masks here; the value only needs to exist
#define VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT 0u
#endif

void FrameTransitionRecorder::RecordTemporalResourceInitialLayouts(CommandBuffer* pCmdBuffer, RGResourceRegistry& registry)
{
    auto transitionInitialTexture = [](CommandBuffer* pCmd, Texture* pTex)
    {
        if (!pTex) return;
        ImageLayoutTransitionCmd cmd(pTex);
        cmd.oldLayout = ImageLayout::UNDEFINED;
        cmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
        TextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::UNDEFINED, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        pCmd->RecordCommand(cmd);
    };

    auto transitionInitialDepthTexture = [](CommandBuffer* pCmd, Texture* pTex)
    {
        if (!pTex) return;
        ImageLayoutTransitionCmd cmd(pTex);
        cmd.oldLayout = ImageLayout::UNDEFINED;
        cmd.newLayout = ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        TextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::UNDEFINED, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        pCmd->RecordCommand(cmd);
    };

    auto setRegistryInitialLayout = [&registry](RGResourceID id, ImageLayout layout)
    {
        RGResourceHandle h = registry.FindByID(id);
        if (h != kInvalidRGHandle)
        {
            registry.SetResourceLayout(h, layout);
            registry.SetHistoryResourceLayout(h, layout);
        }
    };

    Texture* pCSM = registry.GetShadowMap().pTexture;
    transitionInitialDepthTexture(pCmdBuffer, pCSM);
    setRegistryInitialLayout(RGResourceID::MainDepth, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    setRegistryInitialLayout(RGResourceID::GBufferVelocity, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    transitionInitialTexture(pCmdBuffer, registry.ResolveByID(RGResourceID::TemporalResolve));
    setRegistryInitialLayout(RGResourceID::TemporalResolve, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    transitionInitialTexture(pCmdBuffer, registry.ResolveByID(RGResourceID::TAAHistory));
    transitionInitialTexture(pCmdBuffer, registry.ResolveHistoryByID(RGResourceID::TAAHistory));
    setRegistryInitialLayout(RGResourceID::TAAHistory, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    Texture* pPostAA = registry.ResolveByID(RGResourceID::GBufferPostAAColor);
    transitionInitialTexture(pCmdBuffer, pPostAA);
    setRegistryInitialLayout(RGResourceID::GBufferPostAAColor, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    Texture* pSSS = registry.ResolveByID(RGResourceID::ScreenSpaceShadows);
    transitionInitialTexture(pCmdBuffer, pSSS);
    setRegistryInitialLayout(RGResourceID::ScreenSpaceShadows, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    static const RGResourceID bloomResIDs[5] = {
        RGResourceID::BloomMip0,
        RGResourceID::BloomMip1,
        RGResourceID::BloomMip2,
        RGResourceID::BloomMip3,
        RGResourceID::BloomMip4
    };
    for (u32 i = 0; i < 5; ++i)
    {
        transitionInitialTexture(pCmdBuffer, registry.ResolveByID(bloomResIDs[i]));
        setRegistryInitialLayout(bloomResIDs[i], ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    }

    transitionInitialTexture(pCmdBuffer, registry.ResolveByID(RGResourceID::RTReflections));
    setRegistryInitialLayout(RGResourceID::RTReflections, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    transitionInitialTexture(pCmdBuffer, registry.ResolveByID(RGResourceID::RTAOOutput));
    setRegistryInitialLayout(RGResourceID::RTAOOutput, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    Texture* pAccum = registry.ResolveByID(RGResourceID::RTAccumulation);
    Texture* pAccumHistory = registry.ResolveHistoryByID(RGResourceID::RTAccumulation);
    transitionInitialTexture(pCmdBuffer, pAccum);
    if (pAccumHistory && pAccumHistory != pAccum)
    {
        transitionInitialTexture(pCmdBuffer, pAccumHistory);
    }
    setRegistryInitialLayout(RGResourceID::RTAccumulation, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    transitionInitialTexture(pCmdBuffer, registry.ResolveByID(RGResourceID::GBufferDebug));
    setRegistryInitialLayout(RGResourceID::GBufferDebug, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    // DLSSPass uploads the exposure value itself when the exposure texture is enabled
    transitionInitialTexture(pCmdBuffer, registry.ResolveByID(RGResourceID::DLSSExposure));
    setRegistryInitialLayout(RGResourceID::DLSSExposure, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
}

void FrameTransitionRecorder::RecordInitialLayoutTransitions(
    CommandBuffer* pCmdBuffer,
    const stltype::fixed_vector<const Texture*, 16>& allGbufferAndSwapchain,
    Texture* pMainDepthTexture,
    Texture* pShadowMapTexture)
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
        TextureManager::SetLayoutBarrierMasks(colorCmd, ImageLayout::UNDEFINED, ImageLayout::COLOR_ATTACHMENT_OPTIMAL);
        pCmdBuffer->RecordCommand(colorCmd);
    }

    stltype::fixed_vector<const Texture*, 2> depthTextures;
    if (pMainDepthTexture != nullptr)
        depthTextures.push_back(pMainDepthTexture);
    if (pShadowMapTexture != nullptr)
        depthTextures.push_back(pShadowMapTexture);

    if (!depthTextures.empty())
    {
        ImageLayoutTransitionCmd depthCmd(stltype::vector<const Texture*>(depthTextures.begin(), depthTextures.end()));
        depthCmd.oldLayout = ImageLayout::UNDEFINED;
        depthCmd.newLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        TextureManager::SetLayoutBarrierMasks(
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
    TextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::TRANSFER_DST_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(cmd);
}

void FrameTransitionRecorder::RecordGBufferToShaderRead(
    CommandBuffer* pCmdBuffer,
    const stltype::fixed_vector<const Texture*, 8>& gbufferTextures,
    Texture* pShadowMapTexture)
{
    ImageLayoutTransitionCmd colorCmd(stltype::vector<const Texture*>(gbufferTextures.begin(), gbufferTextures.end()));
    colorCmd.oldLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    colorCmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    TextureManager::SetLayoutBarrierMasks(
        colorCmd, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(colorCmd);

    if (pShadowMapTexture)
    {
        ImageLayoutTransitionCmd shadowCmd(pShadowMapTexture);
        shadowCmd.oldLayout = ImageLayout::UNDEFINED;
        shadowCmd.newLayout = ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        TextureManager::SetLayoutBarrierMasks(
            shadowCmd, ImageLayout::UNDEFINED, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        pCmdBuffer->RecordCommand(shadowCmd);
    }
}

void FrameTransitionRecorder::RecordVelocityClear(CommandBuffer* pCmdBuffer, Texture* pVelocityTexture)
{
    if (pVelocityTexture)
    {
        RecordClearColorTexture(pCmdBuffer,
                                pVelocityTexture,
                                ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
                                ImageLayout::COLOR_ATTACHMENT_OPTIMAL);
    }
}

void FrameTransitionRecorder::RecordDepthToReadOnly(CommandBuffer* pCmdBuffer, Texture* pMainDepthTexture)
{
    if (pMainDepthTexture)
    {
        ImageLayoutTransitionCmd depthCmd(pMainDepthTexture);
        depthCmd.oldLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthCmd.newLayout = ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        TextureManager::SetLayoutBarrierMasks(
            depthCmd, ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        pCmdBuffer->RecordCommand(depthCmd);
    }
}

void FrameTransitionRecorder::RecordThisFrameColorToRead(CommandBuffer* pCmdBuffer, Texture* pThisFrameColorTexture)
{
    if (!pThisFrameColorTexture) return;
    ImageLayoutTransitionCmd cmd(pThisFrameColorTexture);
    cmd.oldLayout = ImageLayout::GENERAL;
    cmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    TextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::GENERAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(cmd);
}

void FrameTransitionRecorder::RecordThisFrameColorToGeneral(CommandBuffer* pCmdBuffer, Texture* pThisFrameColorTexture)
{
    if (!pThisFrameColorTexture) return;
    ImageLayoutTransitionCmd cmd(pThisFrameColorTexture);
    cmd.oldLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    cmd.newLayout = ImageLayout::GENERAL;
    TextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::SHADER_READ_ONLY_OPTIMAL, ImageLayout::GENERAL);
    pCmdBuffer->RecordCommand(cmd);
}

void FrameTransitionRecorder::RecordThisFrameColorToGeneralDiscard(CommandBuffer* pCmdBuffer, Texture* pThisFrameColorTexture)
{
    if (!pThisFrameColorTexture) return;
    ImageLayoutTransitionCmd cmd(pThisFrameColorTexture);
    cmd.oldLayout = ImageLayout::UNDEFINED;
    cmd.newLayout = ImageLayout::GENERAL;
    TextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::UNDEFINED, ImageLayout::GENERAL);
    pCmdBuffer->RecordCommand(cmd);
}

void FrameTransitionRecorder::RecordThisFrameColorFromGeneralToRead(CommandBuffer* pCmdBuffer, Texture* pThisFrameColorTexture)
{
    if (!pThisFrameColorTexture) return;
    ImageLayoutTransitionCmd cmd(pThisFrameColorTexture);
    cmd.oldLayout = ImageLayout::GENERAL;
    cmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    TextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::GENERAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(cmd);
}

void FrameTransitionRecorder::RecordClearColorTexture(CommandBuffer* pCmdBuffer,
                                                      Texture* pTexture,
                                                      ImageLayout oldLayout,
                                                      ImageLayout finalLayout)
{
    if (!pTexture) return;

    ImageLayoutTransitionCmd toTransfer(pTexture);
    toTransfer.oldLayout = oldLayout;
    toTransfer.newLayout = ImageLayout::TRANSFER_DST_OPTIMAL;
    TextureManager::SetLayoutBarrierMasks(toTransfer, oldLayout, ImageLayout::TRANSFER_DST_OPTIMAL);
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
    TextureManager::SetLayoutBarrierMasks(toFinal, ImageLayout::TRANSFER_DST_OPTIMAL, finalLayout);
    pCmdBuffer->RecordCommand(toFinal);
}

void FrameTransitionRecorder::RecordSSSOutputToGeneral(CommandBuffer* pCmdBuffer, Texture* pScreenSpaceShadowTexture)
{
    if (!pScreenSpaceShadowTexture)
        return;
    ImageLayoutTransitionCmd cmd(pScreenSpaceShadowTexture);
    cmd.oldLayout = ImageLayout::UNDEFINED;
    cmd.newLayout = ImageLayout::GENERAL;
    TextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::UNDEFINED, ImageLayout::GENERAL);
    pCmdBuffer->RecordCommand(cmd);
}

void FrameTransitionRecorder::RecordSSSOutputToShaderRead(CommandBuffer* pCmdBuffer, Texture* pScreenSpaceShadowTexture)
{
    if (!pScreenSpaceShadowTexture)
        return;
    ImageLayoutTransitionCmd cmd(pScreenSpaceShadowTexture);
    cmd.oldLayout = ImageLayout::GENERAL;
    cmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    TextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::GENERAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    cmd.dstStage = static_cast<SyncStages>(VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT);
    cmd.dstAccessMask = static_cast<AccessFlags>(0);
    pCmdBuffer->RecordCommand(cmd);
}

void FrameTransitionRecorder::RecordSwapchainToAttachment(CommandBuffer* pCmdBuffer, Texture* pSwapchainTexture)
{
    if (!pSwapchainTexture) return;
    ImageLayoutTransitionCmd cmd(pSwapchainTexture);
    cmd.oldLayout = ImageLayout::UNDEFINED;
    cmd.newLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    TextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::UNDEFINED, ImageLayout::COLOR_ATTACHMENT_OPTIMAL);
    pCmdBuffer->RecordCommand(cmd);
}

void FrameTransitionRecorder::RecordSwapchainToPresent(CommandBuffer* pCmdBuffer, Texture* pSwapchainTexture)
{
    if (!pSwapchainTexture) return;
    ImageLayoutTransitionCmd cmd(pSwapchainTexture);
    cmd.oldLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    cmd.newLayout = ImageLayout::PRESENT_SRC_KHR;
    TextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, ImageLayout::PRESENT_SRC_KHR);
    pCmdBuffer->RecordCommand(cmd);
}
