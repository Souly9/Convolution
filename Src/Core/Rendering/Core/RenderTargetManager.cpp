#include "RenderTargetManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/Nvidia/StreamlineManager.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/Core/Utils/DeleteQueue.h"

void RenderTargetManager::Recreate(const mathstl::Vector2& renderResolution, const mathstl::Vector2& outputResolution)
{
    stltype::vector<TextureHandle> oldTextureHandles;

    ColorAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.format = SWAPCHAIN_FORMAT;
    colorAttachmentInfo.finalLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    auto colorAttachment = ColorAttachment::Create(colorAttachmentInfo);

    m_attachments.colorAttachments[RenderPasses::ColorAttachmentType::GBufferColor].clear();
    m_attachments.colorAttachments[RenderPasses::ColorAttachmentType::GBufferColor].push_back(colorAttachment);

    CreateDepthAttachment(renderResolution, oldTextureHandles);
    RecreateGBufferTextures(renderResolution, outputResolution, oldTextureHandles);
    RecreateAuxiliaryTextures(renderResolution, outputResolution, oldTextureHandles);
    RecreateDLSSExposureTexture();

    if (!oldTextureHandles.empty())
    {
        g_pDeleteQueue->RegisterDeleteForNextFrame(
            [oldTextureHandles = stltype::move(oldTextureHandles)]() mutable
            {
                for (const auto handle : oldTextureHandles)
                {
                    if (handle != 0)
                    {
                        g_pTexManager->FreeTexture(handle);
                    }
                }
            });
    }
}

void RenderTargetManager::RotateHistory()
{
    m_gbuffer.FlipHistoryBuffers();
    stltype::swap(m_pDepthTexture, m_pLastFrameDepthTexture);
    stltype::swap(m_depthBindlessHandle, m_lastFrameDepthBindlessHandle);
    m_attachments.depthAttachment.SetTexture(m_pDepthTexture);
}

void RenderTargetManager::CreateDepthAttachment(const mathstl::Vector2& renderResolution,
                                                stltype::vector<TextureHandle>& oldTextureHandles)
{
    if (m_depthTextureHandle != 0)
        oldTextureHandles.push_back(m_depthTextureHandle);
    if (m_lastFrameDepthTextureHandle != 0)
        oldTextureHandles.push_back(m_lastFrameDepthTextureHandle);

    DepthBufferAttachmentInfo depthAttachmentInfo{};
    depthAttachmentInfo.format = DEPTH_BUFFER_FORMAT;

    DynamicTextureRequest depthRequest{};
    depthRequest.handle = g_pTexManager->GenerateHandle();
    depthRequest.extents = DirectX::XMUINT3(renderResolution.x, renderResolution.y, 1);
    depthRequest.format = DEPTH_BUFFER_FORMAT;
    depthRequest.usage = Usage::Sampled | Usage::DepthAttachment | Usage::TransferDst | Usage::TransferSrc;
    depthRequest.isPersistent = true;
    depthRequest.AddName("Main Depth Buffer");

    m_depthTextureHandle = depthRequest.handle;
    m_pDepthTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(depthRequest));
    m_depthBindlessHandle = g_pTexManager->MakeTextureBindless(depthRequest.handle, depthRequest.isPersistent);

    depthRequest.handle = g_pTexManager->GenerateHandle();
    depthRequest.AddName("Last Frame Depth Buffer");

    m_lastFrameDepthTextureHandle = depthRequest.handle;
    m_pLastFrameDepthTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(depthRequest));
    m_lastFrameDepthBindlessHandle = g_pTexManager->MakeTextureBindless(depthRequest.handle, depthRequest.isPersistent);

    m_attachments.depthAttachment = DepthAttachment::Create(depthAttachmentInfo, m_pDepthTexture);
}

void RenderTargetManager::RecreateGBufferTextures(const mathstl::Vector2& renderResolution,
                                                  const mathstl::Vector2& outputResolution,
                                                  stltype::vector<TextureHandle>& oldTextureHandles)
{
    DynamicTextureRequest baseRequest{};
    baseRequest.extents = DirectX::XMUINT3(renderResolution.x, renderResolution.y, 1);
    baseRequest.usage = Usage::GBuffer | Usage::TransferSrc | Usage::TransferDst;
    baseRequest.isPersistent = true;

    struct GBufferTexDef
    {
        GBufferTextureType type;
        const char* name;
        Usage extraUsage = Usage::None;
        bool nearest = true;
        bool outputSized = false;
    };

    static const GBufferTexDef defs[] = {
        {GBufferTextureType::GBufferAlbedo, "GBuffer Albedo"},
        {GBufferTextureType::GBufferNormal, "GBuffer Normal"},
        {GBufferTextureType::TexCoordMatData, "GBuffer UV Material Data"},
        {GBufferTextureType::GBufferDebug, "GBuffer Debug"},
        {GBufferTextureType::GBufferVelocity, "GBuffer Velocity", Usage::None, false},
        {GBufferTextureType::GBufferLastFrameVelocity, "GBuffer Last Frame Velocity", Usage::None, false},
        {GBufferTextureType::GBufferLastFrameColor, "GBuffer Last Frame Color", Usage::Storage, false, true},
        {GBufferTextureType::GBufferThisFrameColor, "GBuffer This Frame Color", Usage::Storage, false},
        {GBufferTextureType::GBufferTemporalCurrentColor,
         "GBuffer Temporal Current Color",
         Usage::Storage | Usage::Sampled,
         false},
        {GBufferTextureType::GBufferResolve, "GBuffer Resolve", Usage::Storage | Usage::Sampled, false, true},
        {GBufferTextureType::GBufferPostAAColor, "GBuffer Post AA Color", Usage::Storage | Usage::Sampled, false, true}};

    for (const auto& def : defs)
    {
        const TextureHandle oldHandle = m_gbuffer.GetTextureHandle(def.type);
        if (oldHandle != 0)
        {
            oldTextureHandles.push_back(oldHandle);
            m_gbuffer.ClearTextureHandle(def.type);
        }

        DynamicTextureRequest req = baseRequest;
        if (def.outputSized)
            req.extents = DirectX::XMUINT3(outputResolution.x, outputResolution.y, 1);
        req.format = m_gbuffer.GetFormat(def.type);
        req.handle = g_pTexManager->GenerateHandle();
        req.usage |= def.extraUsage;
        req.AddName(def.name);
        if (def.nearest)
        {
            req.samplerInfo.minFilter = TextureFilter::NEAREST;
            req.samplerInfo.magFilter = TextureFilter::NEAREST;
        }

        m_gbuffer.Set(def.type, static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(req)));
        m_gbuffer.SetTextureHandle(def.type, req.handle);
        m_gbuffer.SetHandle(def.type, g_pTexManager->MakeTextureBindless(req.handle, true));
    }
}

void RenderTargetManager::RecreateAuxiliaryTextures(const mathstl::Vector2& renderResolution,
                                                    const mathstl::Vector2& outputResolution,
                                                    stltype::vector<TextureHandle>& oldTextureHandles)
{
    DynamicTextureRequest baseRequest{};
    baseRequest.extents = DirectX::XMUINT3(renderResolution.x, renderResolution.y, 1);
    baseRequest.usage = Usage::GBuffer | Usage::TransferSrc | Usage::TransferDst;
    baseRequest.isPersistent = true;

    DynamicTextureRequest sssRequest = baseRequest;
    sssRequest.format = TexFormat::R8_UNORM;
    sssRequest.handle = g_pTexManager->GenerateHandle();
    sssRequest.AddName("Screen Space Shadows");
    sssRequest.usage = Usage::Storage | Usage::Sampled;
    if (m_pScreenSpaceShadowTexture)
        oldTextureHandles.push_back(m_screenSpaceShadowsTextureHandle);
    m_pScreenSpaceShadowTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(sssRequest));
    m_screenSpaceShadowsTextureHandle = sssRequest.handle;
    m_screenSpaceShadowBindlessHandle = g_pTexManager->MakeTextureBindless(sssRequest.handle, true);
    m_attachments.pScreenSpaceShadowTexture = m_pScreenSpaceShadowTexture;

    DynamicTextureRequest smaaEdgeReq = baseRequest;
    smaaEdgeReq.extents = DirectX::XMUINT3(outputResolution.x, outputResolution.y, 1);
    smaaEdgeReq.format = TexFormat::R8G8_UNORM;
    smaaEdgeReq.handle = g_pTexManager->GenerateHandle();
    smaaEdgeReq.AddName("SMAA Edges");
    smaaEdgeReq.usage = Usage::GBuffer | Usage::Storage | Usage::Sampled;
    if (m_pSMAAEdgesTexture)
        oldTextureHandles.push_back(m_smaaEdgesTextureHandle);
    m_pSMAAEdgesTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(smaaEdgeReq));
    m_smaaEdgesTextureHandle = smaaEdgeReq.handle;
    m_smaaEdgesBindlessHandle = g_pTexManager->MakeTextureBindless(smaaEdgeReq.handle, true);

    DynamicTextureRequest smaaBlendReq = baseRequest;
    smaaBlendReq.extents = DirectX::XMUINT3(outputResolution.x, outputResolution.y, 1);
    smaaBlendReq.format = TexFormat::R8G8B8A8_UNORM;
    smaaBlendReq.handle = g_pTexManager->GenerateHandle();
    smaaBlendReq.AddName("SMAA Blend Weights");
    smaaBlendReq.usage = Usage::GBuffer | Usage::Storage | Usage::Sampled;
    if (m_pSMAABlendTexture)
        oldTextureHandles.push_back(m_smaaBlendTextureHandle);
    m_pSMAABlendTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(smaaBlendReq));
    m_smaaBlendTextureHandle = smaaBlendReq.handle;
    m_smaaBlendBindlessHandle = g_pTexManager->MakeTextureBindless(smaaBlendReq.handle, true);

    m_attachments.pSMAAEdgesTexture = m_pSMAAEdgesTexture;
    m_attachments.pSMAABlendTexture = m_pSMAABlendTexture;
}

void RenderTargetManager::RecreateDLSSExposureTexture()
{
    if (m_dlssExposureTextureHandle != 0)
    {
        g_pTexManager->FreeTexture(m_dlssExposureTextureHandle);
        m_dlssExposureTextureHandle = 0;
        m_pDLSSExposureTexture = nullptr;
    }

    if (!Nvidia::StreamlineManager::IsDLSSSupported())
        return;

    DynamicTextureRequest exposureReq{};
    exposureReq.extents = {1, 1, 1};
    exposureReq.handle = g_pTexManager->GenerateHandle();
    exposureReq.format = TexFormat::R32_FLOAT;
    exposureReq.usage = Usage::Sampled | Usage::TransferDst;
    exposureReq.isPersistent = true;
    exposureReq.hasMipMaps = false;
    exposureReq.mipLevels = 1;
    exposureReq.createSampler = false;
    exposureReq.AddName("DLSS Exposure");
    m_pDLSSExposureTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(exposureReq));
    m_dlssExposureTextureHandle = exposureReq.handle;
    m_dlssExposureStagingBuffer.EnsureCapacity(sizeof(float));
}
