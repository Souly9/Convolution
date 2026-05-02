#include "PassManager.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "ClusteredShading/ClusterDebugPass.h"
#include "ClusteredShading/ClusterGeneratorComputePass.h"
#include "ClusteredShading/LightGridComputePass.h"
#include "ClusteredShading/LightTransformComputePass.h"
#include "ClusteredShading/TileAssignmentComputePass.h"
#include "Compositing/CompositPass.h"
#include "Core/Global/FrameGlobals.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/GPUTimingQuery.h"
#include "Core/Rendering/Core/Nvidia/StreamlineManager.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Core/Utils/DeleteQueue.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"
#include "Core/Rendering/Vulkan/Utils/VkEnumHelpers.h"
#include "DebugShapePass.h"
#include "ImGuiPass.h"
#include "PreProcess/DepthPrePass.h"
#include "ScreenSpaceShadowPass.h"
#include "ShadowPass.h"
#include "StaticMeshPass.h"
#include "AA/TAAPass.h"
#include "AA/DLSSPass.h"
#include "Compositing/LightingPass.h"
#include "RT/RTDebugViewPass.h"
#include "Core/Rendering/Core/ProfilingUtils.h"
#include "vulkan/vulkan_core.h"
#include <cstring>
#include <imgui/backends/imgui_impl_vulkan.h>

using namespace RenderPasses;

namespace
{
mathstl::Vector2 CalculateRenderResolution(const mathstl::Vector2& swapchainResolution, u32 upscalingPercentage)
{
    const f32 scale = static_cast<f32>(upscalingPercentage) / 100.0f;
    return {static_cast<f32>(stltype::max(1u, static_cast<u32>(swapchainResolution.x * scale))),
            static_cast<f32>(stltype::max(1u, static_cast<u32>(swapchainResolution.y * scale)))};
}
}

// Helper implementations to break up large functions
void PassManager::InitResourceManagerAndCallbacks()
{
    m_resourceManager.Init();
    m_rtSceneManager.Init(&m_resourceManager, VkGlobals::GetQueueFamilyIndices().graphicsFamily.value());
    g_pEventSystem->AddSceneLoadedEventCallback(
        [this](const SceneLoadedEventData&)
        {
            m_rtSceneManager.Reset();
            m_resourceManager.UploadSceneGeometry(g_pMeshManager->GetMeshes());
            m_rtSceneManager.RegisterSceneMeshes(g_pMeshManager->GetMeshes());
        });
    g_pEventSystem->AddShaderHotReloadEventCallback([this](const auto&) { RebuildPipelinesForAllPasses(); });
    g_pEventSystem->AddSwapchainRecreatedEventCallback(
        [this](const SwapchainRecreatedEventData& data)
        {
            SimpleScopedGuard<CustomMutex> lock(m_swapchainEventMutex);
            m_pendingSwapchainRecreatedEvent = data;
            m_hasPendingSwapchainRecreatedEvent = true;
        });

    // Create pass objects
    AddPass(PassType::LightTransformCompute, stltype::make_unique<RenderPasses::LightTransformComputePass>());
    AddPass(PassType::TileAssignmentCompute, stltype::make_unique<RenderPasses::TileAssignmentComputePass>());
    AddPass(PassType::ClusterGenCompute, stltype::make_unique<RenderPasses::ClusterGeneratorComputePass>());
    AddPass(PassType::EarlyAsyncCompute, stltype::make_unique<RenderPasses::LightGridComputePass>());
    AddPass(PassType::PreProcess, stltype::make_unique<RenderPasses::DepthPrePass>());
    AddPass(PassType::DepthReliantCompute, stltype::make_unique<RenderPasses::ScreenSpaceShadowPass>());
    AddPass(PassType::Main, stltype::make_unique<RenderPasses::StaticMainMeshPass>());
    AddPass(PassType::Main, stltype::make_unique<RenderPasses::DebugShapePass>());
    AddPass(PassType::Shadow, stltype::make_unique<RenderPasses::CSMPass>());
    AddPass(PassType::Debug, stltype::make_unique<RenderPasses::ClusterDebugPass>());
    AddPass(PassType::Debug, stltype::make_unique<RenderPasses::ClusterDebugPass>());
    AddPass(PassType::UI, stltype::make_unique<RenderPasses::ImGuiPass>());
    AddPass(PassType::Lighting, stltype::make_unique<RenderPasses::LightingPass>());
    auto pRTDebugPass = stltype::make_unique<RenderPasses::RTDebugViewPass>();
    m_pRTDebugViewPass = pRTDebugPass.get();
    AddPass(PassType::PostProcess, std::move(pRTDebugPass));
    AddPass(PassType::TAA, stltype::make_unique<RenderPasses::TAAPass>());
    if (Nvidia::StreamlineManager::IsDLSSSupported())
    {
        AddPass(PassType::DLSS, stltype::make_unique<RenderPasses::DLSSPass>());
    }
    AddPass(PassType::Composite, stltype::make_unique<RenderPasses::CompositPass>());
}

void PassManager::CreateUBOsAndMap()
{
    m_frameResourceManager.Init();
    m_frameResourceManager.CreatePassObjectsAndLayouts();
    m_frameResourceManager.CreateFrameRendererContexts(
        m_imageAvailableSemaphores, m_imageAvailableFences, m_renderFinishedFences);
}

void PassManager::InitPassesAndImGui()
{
    SyncRenderStateFromAppState();
    RecreateAllRenderResources();
}

void PassManager::CreateDepthAttachment()
{
    if (m_depthTextureHandle != 0)
        g_pTexManager->FreeTexture(m_depthTextureHandle);
    if (m_lastFrameDepthTextureHandle != 0)
        g_pTexManager->FreeTexture(m_lastFrameDepthTextureHandle);

    DepthBufferAttachmentInfo depthAttachmentInfo{};
    depthAttachmentInfo.format = DEPTH_BUFFER_FORMAT;

    DynamicTextureRequest depthRequest{};
    depthRequest.handle = g_pTexManager->GenerateHandle();
    depthRequest.extents =
        DirectX::XMUINT3(m_renderState.renderResolution.x, m_renderState.renderResolution.y, 1);
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

    m_globalRendererAttachments.depthAttachment = DepthAttachment::Create(depthAttachmentInfo, m_pDepthTexture);
}

void PassManager::RecreateAllRenderResources()
{
    ScopedZone("PassManager::RecreateAllRenderResources");

    SyncRenderStateFromAppState();
    m_renderState.recreatedThisFrame = true;

    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;

    // 1. Recreate Shadow Maps
    {
        const auto csmCascades = renderState.directionalLightCascades;
        const auto csmResolution = renderState.csmResolution;
        RecreateShadowMaps(csmCascades, csmResolution);
        auto& shadowMapState = m_frameResourceManager.GetShadowMapState();
        shadowMapState.cascadeCount = csmCascades;
        shadowMapState.shadowMapExtents = csmResolution;
    }

    // 2. Recreate Main Pass Resolve Attachment
    ColorAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.format = SWAPCHAIN_FORMAT;
    colorAttachmentInfo.finalLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    auto colorAttachment = ColorAttachment::Create(colorAttachmentInfo);

    m_globalRendererAttachments.colorAttachments[ColorAttachmentType::GBufferColor].clear();
    m_globalRendererAttachments.colorAttachments[ColorAttachmentType::GBufferColor].push_back(colorAttachment);

    // 3. Recreate Depth
    CreateDepthAttachment();

    // 4. Recreate G-Buffers (updates descriptors internally)
    RecreateGbuffers(m_renderState.renderResolution);

    // 5. Update Main Pass Data with new handles
    u32 idx = 0;
    for (auto& mainPassData : m_mainPassData)
    {
        mainPassData.bufferDescriptors[UBO::DescriptorContentsType::GlobalInstanceData] =
            m_resourceManager.GetInstanceSSBODescriptorSet(idx);
        mainPassData.bufferDescriptors[UBO::DescriptorContentsType::LightData] =
            m_frameResourceManager.GetFrameRendererContext(idx).tileArraySSBODescriptor;
        mainPassData.bufferDescriptors[UBO::DescriptorContentsType::GBuffer] =
            m_frameResourceManager.GetFrameRendererContext(idx).gbufferPostProcessDescriptor;
        mainPassData.bufferDescriptors[UBO::DescriptorContentsType::BindlessTextureArray] =
            DescriptorSet::Cast(g_pTexManager->GetBindlessDescriptorSet());
        mainPassData.bufferDescriptors[UBO::DescriptorContentsType::BindlessImageArray] =
            DescriptorSet::Cast(g_pTexManager->GetBindlessImageDescriptorSet());
        mainPassData.bufferDescriptors[UBO::DescriptorContentsType::ClusterGrid] =
            m_frameResourceManager.GetFrameRendererContext(idx).clusterGridDescriptor;

        mainPassData.renderState = m_renderState;
        UpdateTemporalResources(mainPassData);
        mainPassData.depthBufferBindlessHandle = mainPassData.temporalResources.currentDepthHandle;
        mainPassData.pMainDepthTexture = mainPassData.temporalResources.pCurrentDepthTexture;
        mainPassData.pLastFrameDepthTexture = mainPassData.temporalResources.pHistoryDepthTexture;
        ++idx;
    }

    // 6. Re-init passes with new attachments
    for (auto& [type, passes] : m_passes)
    {
        for (auto& pPass : passes)
        {
            pPass->Init(m_globalRendererAttachments, m_resourceManager);
        }
    }

    // 7. Update UI Descriptors
    RegisterImGuiTextures();
    UpdateImGuiGbufferTextures();
}

bool PassManager::AnyPassWantsToRender() const
{
    for (const auto& [type, passes] : m_passes)
    {
        for (const auto& pass : passes)
        {
            if (pass->WantsToRender())
                return true;
        }
    }
    return false;
}

void PassManager::PrepareMainPassDataForFrame(MainPassData& mainPassData, FrameRendererContext& ctx, u32 frameIdx)
{
    mainPassData.pResourceManager = &m_resourceManager;
    mainPassData.pGbuffer = &m_gbuffer;
    mainPassData.mainView.descriptorSet = ctx.sharedDataUBODescriptor;
    mainPassData.renderState = m_renderState;
    mainPassData.directionalLightShadowMap = m_globalRendererAttachments.directionalLightShadowMap;
    mainPassData.cascades = m_frameResourceManager.GetShadowMapState().cascadeCount;
    mainPassData.depthBufferBindlessHandle = mainPassData.temporalResources.currentDepthHandle;
    mainPassData.pMainDepthTexture = mainPassData.temporalResources.pCurrentDepthTexture;
    mainPassData.pLastFrameDepthTexture = mainPassData.temporalResources.pHistoryDepthTexture;

    mainPassData.bufferDescriptors[UBO::DescriptorContentsType::GlobalInstanceData] =
        m_resourceManager.GetInstanceSSBODescriptorSet(frameIdx);
    mainPassData.bufferDescriptors[UBO::DescriptorContentsType::BindlessTextureArray] =
        g_pTexManager->GetBindlessDescriptorSet();
    mainPassData.bufferDescriptors[UBO::DescriptorContentsType::ClusterGrid] = ctx.clusterGridDescriptor;
    mainPassData.pRTSceneManager = &m_rtSceneManager;
    mainPassData.rtDebugTextureHandle = m_pRTDebugViewPass ? m_pRTDebugViewPass->GetOutputBindlessHandle() : 0;

    mainPassData.pSMAAEdgesTexture = m_pSMAAEdgesTexture;
    mainPassData.pSMAABlendTexture = m_pSMAABlendTexture;
    mainPassData.smaaEdges = m_smaaEdgesBindlessHandle;
    mainPassData.smaaBlend = m_smaaBlendBindlessHandle;

    ctx.pDLSSExposureTexture = m_pDLSSExposureTexture;
}

void PassManager::UpdateTemporalResources(MainPassData& mainPassData)
{
    auto& temporal = mainPassData.temporalResources;
    temporal.pCurrentColorTexture = m_gbuffer.Get(GBufferTextureType::GBufferThisFrameColor);
    temporal.pHistoryColorTexture = m_gbuffer.Get(GBufferTextureType::GBufferLastFrameColor);
    temporal.pResolveTexture = m_gbuffer.Get(GBufferTextureType::GBufferResolve);
    temporal.pCurrentDepthTexture = m_pDepthTexture;
    temporal.pHistoryDepthTexture = m_pLastFrameDepthTexture;
    temporal.currentColorHandle = m_gbuffer.GetHandle(GBufferTextureType::GBufferThisFrameColor);
    temporal.historyColorHandle = m_gbuffer.GetHandle(GBufferTextureType::GBufferLastFrameColor);
    temporal.resolveHandle = m_gbuffer.GetHandle(GBufferTextureType::GBufferResolve);
    temporal.currentDepthHandle = m_depthBindlessHandle;
    temporal.historyDepthHandle = m_lastFrameDepthBindlessHandle;
}

void PassManager::InitFrameContexts()
{
    const auto& indices = VkGlobals::GetQueueFamilyIndices();

    if (!m_graphicsFrameCtx.initialized)
    {
        m_graphicsFrameCtx.cmdPool = CommandPool::Create(indices.graphicsFamily.value());
        m_graphicsFrameCtx.cmdPool.SetName("Graphics Command Pool");
        for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
        {
            const auto numberString = stltype::to_string(i);
            m_graphicsFrameCtx.cmdBuffers[i] =
                m_graphicsFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
            m_graphicsFrameCtx.cmdBuffers[i]->SetName("Main Graphics Command Buffer " + numberString);
            m_graphicsFrameCtx.compositeCmdBuffers[i] =
                m_graphicsFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
            m_graphicsFrameCtx.compositeCmdBuffers[i]->SetName("Composite Graphics Command Buffer " + numberString);
            m_graphicsFrameCtx.depthPrePassCmdBuffers[i] =
                m_graphicsFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
            m_graphicsFrameCtx.depthPrePassCmdBuffers[i]->SetName("Depth Pre-Pass Graphics Command Buffer " + numberString);
        }
        m_graphicsFrameCtx.initialized = true;
    }

    if (!m_computeFrameCtx.initialized)
    {
        m_computeFrameCtx.cmdPool = CommandPool::Create(indices.computeFamily.value());
        m_computeFrameCtx.cmdPool.SetName("Compute Command Pool");
        for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
        {
            const auto numberString = stltype::to_string(i);
            m_computeFrameCtx.cmdBuffers[i] = m_computeFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
            m_computeFrameCtx.cmdBuffers[i]->SetName("Async Compute Command Buffer " + numberString);
            m_computeFrameCtx.sssComputeCmdBuffers[i] =
                m_computeFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
            m_computeFrameCtx.sssComputeCmdBuffers[i]->SetName("SSS Compute Command Buffer " + numberString);
        }
        m_computeFrameCtx.initialized = true;
    }
}

void PassManager::RenderPassGroup(PassType groupType,
                                  const MainPassData& data,
                                  FrameRendererContext& ctx,
                                  CommandBuffer* pCmdBuffer)
{
    const auto it = m_passes.find(groupType);
    if (it == m_passes.end())
        return;

    const auto& passes = it->second;
    if (passes.empty())
        return;

    for (auto& pass : passes)
    {
        if (pass->WantsToRender())
            pass->Render(data, ctx, pCmdBuffer);
    }
}

void PassManager::RenderAllPassGroups(const MainPassData& mainPassData,
                                      FrameRendererContext& ctx,
                                      Semaphore& imageAvailableSemaphore)
{
    ScopedZone("PassManager::RenderAllPassGroups");

    UpdateGBufferUBO(mainPassData);

    if (m_gpuTimingQuery.IsEnabled())
    {
        m_gpuTimingQuery.ResetQueries(ctx.currentFrame);
    }

    stltype::fixed_vector<const Texture*, 8> gbufferTextures;
    m_gbuffer.GetGeometryOutputTextures(gbufferTextures);

    stltype::fixed_vector<const Texture*, 16> allColorTextures;
    allColorTextures.assign(gbufferTextures.begin(), gbufferTextures.end());
    allColorTextures.push_back(m_gbuffer.Get(GBufferTextureType::GBufferThisFrameColor));
    allColorTextures.push_back(m_pSMAAEdgesTexture);
    allColorTextures.push_back(m_pSMAABlendTexture);
    allColorTextures.push_back(ctx.pCurrentSwapchainTexture);

    CommandBuffer* pMainGraphicsWorkBuffer = m_graphicsFrameCtx.cmdBuffers[ctx.imageIdx];
    CommandBuffer* pComputeCmdBuffer = m_computeFrameCtx.cmdBuffers[ctx.imageIdx];
    CommandBuffer* pDepthWorkBuffer = m_graphicsFrameCtx.depthPrePassCmdBuffers[ctx.imageIdx];

    pMainGraphicsWorkBuffer->ResetBuffer();
    pComputeCmdBuffer->ResetBuffer();
    pDepthWorkBuffer->ResetBuffer();

    pMainGraphicsWorkBuffer->SetFrameIdx(ctx.currentFrame);
    pComputeCmdBuffer->SetFrameIdx(ctx.currentFrame);
    pDepthWorkBuffer->SetFrameIdx(ctx.currentFrame);

    auto pendingFlips = m_resourceManager.PopPendingVisibleInstanceIndices();

    stltype::fixed_vector<u64, SWAPCHAIN_IMAGES> priorCompositeValues;
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        priorCompositeValues.push_back(m_frameResourceManager.GetFrameRendererContext(i).nextTimelineValue);
    }

    u64 computeSignalValue = s_globalTimelineCounter.fetch_add(5);
    u64 depthPassSignalValue = computeSignalValue + 1;
    u64 sssSignalValue = computeSignalValue + 2;
    u64 mainPassSignalValue = computeSignalValue + 3;
    u64 graphicsTimelineValue = computeSignalValue + 4;

    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        if (priorCompositeValues[i] == 0)
            continue;

        auto& priorCtx = m_frameResourceManager.GetFrameRendererContext(i);
        pComputeCmdBuffer->AddTimelineWait(&priorCtx.frameTimeline, priorCompositeValues[i]);
        pDepthWorkBuffer->AddTimelineWait(&priorCtx.frameTimeline, priorCompositeValues[i]);
    }

    // Dispatch early async compute work
    {
        RenderPassGroup(PassType::LightTransformCompute, mainPassData, ctx, pComputeCmdBuffer);

        {
            Profiling::StartScope(pComputeCmdBuffer, &m_gpuTimingQuery, m_clearTileCountersTimingIndex, "Clearing Previous Tile Data");

            pComputeCmdBuffer->RecordCommand(GlobalBarrierCmd(SyncStages::COMPUTE_SHADER,
                                                              SyncStages::COMPUTE_SHADER,
                                                              VK_ACCESS_SHADER_WRITE_BIT,
                                                              VK_ACCESS_SHADER_READ_BIT));

            constexpr u64 tileCountersOffset = MAX_SCENE_LIGHTS * sizeof(mathstl::Vector4);
            pComputeCmdBuffer->RecordCommand(BufferFillCmd(&m_resourceManager.GetViewSpaceLightsSSBO(),
                                                            tileCountersOffset,
                                                            UBO::ViewSpaceLightsSSBOSize - tileCountersOffset,
                                                            0));

            pComputeCmdBuffer->RecordCommand(GlobalBarrierCmd(SyncStages::TRANSFER,
                                                              SyncStages::COMPUTE_SHADER,
                                                              VK_ACCESS_TRANSFER_WRITE_BIT,
                                                              VK_ACCESS_SHADER_READ_BIT));

            Profiling::EndScope(pComputeCmdBuffer, &m_gpuTimingQuery, m_clearTileCountersTimingIndex);
        }

        RenderPassGroup(PassType::TileAssignmentCompute, mainPassData, ctx, pComputeCmdBuffer);
        RenderPassGroup(PassType::ClusterGenCompute, mainPassData, ctx, pComputeCmdBuffer);

        pComputeCmdBuffer->RecordCommand(GlobalBarrierCmd(SyncStages::COMPUTE_SHADER,
                                                           SyncStages::COMPUTE_SHADER,
                                                           VK_ACCESS_SHADER_WRITE_BIT,
                                                           VK_ACCESS_SHADER_READ_BIT));

        RenderPassGroup(PassType::EarlyAsyncCompute, mainPassData, ctx, pComputeCmdBuffer);

        pComputeCmdBuffer->AddTimelineSignal(&ctx.computeTimeline, computeSignalValue);
        pComputeCmdBuffer->SetSignalStages(SyncStages::COMPUTE_SHADER);
        pComputeCmdBuffer->Bake();
        g_pQueueHandler->SubmitCommandBufferThisFrame({pComputeCmdBuffer, QueueType::Compute, ctx.currentFrame});
    }

    // Depth Pre-Pass
    {
        if (Nvidia::StreamlineManager::IsDLSSSupported())
        {
            RecordDLSSExposureUpdate(pDepthWorkBuffer);
        }

        if (!pendingFlips.empty())
        {
            Profiling::StartScope(pDepthWorkBuffer,
                                  &m_gpuTimingQuery,
                                  m_instanceBufferUpdateTimingIndex,
                                  "Instance Buffer Updates");

            for (u32 idx : pendingFlips)
            {
                u64 offset = (u64)idx * sizeof(UBO::InstanceData) + offsetof(UBO::InstanceData, flags);
                pDepthWorkBuffer->RecordCommand(BufferUpdateCmd(&m_resourceManager.GetInstanceBuffer(), offset, 1));
            }

            pDepthWorkBuffer->RecordCommand(
                GlobalBarrierCmd(SyncStages::TRANSFER,
                                 SyncStages::VERTEX_SHADER | SyncStages::FRAGMENT_SHADER | SyncStages::COMPUTE_SHADER,
                                 VK_ACCESS_TRANSFER_WRITE_BIT,
                                 VK_ACCESS_SHADER_READ_BIT));

            Profiling::EndScope(pDepthWorkBuffer, &m_gpuTimingQuery, m_instanceBufferUpdateTimingIndex);
        }

        RecordInitialLayoutTransitions(pDepthWorkBuffer, allColorTextures);
        RenderPassGroup(PassType::PreProcess, mainPassData, ctx, pDepthWorkBuffer);
        RecordDepthToReadOnly(pDepthWorkBuffer);
        pDepthWorkBuffer->AddTimelineSignal(&ctx.frameTimeline, depthPassSignalValue);
        pDepthWorkBuffer->SetSignalStages(SyncStages::DEPTH_OUTPUT);
        pDepthWorkBuffer->Bake();
        g_pQueueHandler->SubmitCommandBufferThisFrame({pDepthWorkBuffer, QueueType::Graphics, ctx.currentFrame});
    }

    // SSS (DepthReliantCompute)
    CommandBuffer* pSSSWorkBuffer = m_computeFrameCtx.sssComputeCmdBuffers[ctx.imageIdx];
    pSSSWorkBuffer->ResetBuffer();
    pSSSWorkBuffer->SetFrameIdx(ctx.currentFrame);
    {
        RecordSSSOutputToGeneral(pSSSWorkBuffer);
        RenderPassGroup(PassType::DepthReliantCompute, mainPassData, ctx, pSSSWorkBuffer);
        RecordSSSOutputToShaderRead(pSSSWorkBuffer);

        pSSSWorkBuffer->AddTimelineWait(&ctx.frameTimeline, depthPassSignalValue);
        pSSSWorkBuffer->SetWaitStages(SyncStages::COMPUTE_SHADER);
        pSSSWorkBuffer->AddTimelineSignal(&ctx.computeTimeline, sssSignalValue);
        pSSSWorkBuffer->SetSignalStages(SyncStages::COMPUTE_SHADER);
        pSSSWorkBuffer->Bake();
        g_pQueueHandler->SubmitCommandBufferThisFrame({pSSSWorkBuffer, QueueType::Compute, ctx.currentFrame});
    }

    // Geometry Stage (Main -> Shadow)
    {
        pMainGraphicsWorkBuffer->AddTimelineWait(&ctx.frameTimeline, depthPassSignalValue);
        pMainGraphicsWorkBuffer->SetWaitStages(SyncStages::TRANSFER |
                                                SyncStages::EARLY_FRAGMENT_TESTS |
                                                SyncStages::COLOR_ATTACHMENT_OUTPUT);
        RecordVelocityClear(pMainGraphicsWorkBuffer);
        RenderPassGroup(PassType::Main, mainPassData, ctx, pMainGraphicsWorkBuffer);
        RenderPassGroup(PassType::Debug, mainPassData, ctx, pMainGraphicsWorkBuffer);
        RenderPassGroup(PassType::Shadow, mainPassData, ctx, pMainGraphicsWorkBuffer);
        
        RecordGBufferToShaderRead(pMainGraphicsWorkBuffer, gbufferTextures);

        pMainGraphicsWorkBuffer->AddTimelineSignal(&ctx.frameTimeline, mainPassSignalValue);
        pMainGraphicsWorkBuffer->SetSignalStages(SyncStages::COLOR_ATTACHMENT_OUTPUT | SyncStages::DEPTH_OUTPUT);
        pMainGraphicsWorkBuffer->Bake();
        g_pQueueHandler->SubmitCommandBufferThisFrame({pMainGraphicsWorkBuffer, QueueType::Graphics, ctx.currentFrame});
    }

    // Final Stage (Lighting -> UI)
    CommandBuffer* pFinalWorkBuffer = m_graphicsFrameCtx.compositeCmdBuffers[ctx.imageIdx];
    pFinalWorkBuffer->ResetBuffer();
    pFinalWorkBuffer->SetFrameIdx(ctx.currentFrame);
    {
        pFinalWorkBuffer->AddTimelineWait(&ctx.frameTimeline, mainPassSignalValue);
        pFinalWorkBuffer->AddTimelineWait(&ctx.computeTimeline, sssSignalValue);
        pFinalWorkBuffer->AddWaitSemaphore(&imageAvailableSemaphore);
        // Final stage includes DLSS evaluation through Streamline, which may record compute work.
        // Wait at both fragment and compute stages so DLSS cannot run before upstream timeline waits resolve.
        pFinalWorkBuffer->SetWaitStages(SyncStages::FRAGMENT_SHADER | SyncStages::COMPUTE_SHADER);

        RenderPassGroup(PassType::Lighting, mainPassData, ctx, pFinalWorkBuffer);

        // ThisFrameColor to SHADER_READ
        RecordThisFrameColorToRead(pFinalWorkBuffer);
        RenderPassGroup(PassType::PostProcess, mainPassData, ctx, pFinalWorkBuffer);

        const auto& appRenderState = g_pApplicationState->GetCurrentApplicationState().renderState;
        const bool taaModeActive = appRenderState.aaType == AntialiasingType::TAA_SMAA;
        const bool seedHistoryFromCurrentColor = taaModeActive && appRenderState.taaSeedHistoryFromCurrentColor;
        const bool debugCopyCurrent = taaModeActive && appRenderState.taaDebugMode == 1u;
        const bool debugCopyHistory = taaModeActive && appRenderState.taaDebugMode == 2u;

        if (taaModeActive)
        {
            if (seedHistoryFromCurrentColor)
            {
                RecordCopyTextureToResolve(pFinalWorkBuffer, m_gbuffer.Get(GBufferTextureType::GBufferThisFrameColor));
                g_pApplicationState->RegisterUpdateFunction(
                    [](ApplicationState& state) { state.renderState.taaSeedHistoryFromCurrentColor = false; });
            }
            else if (debugCopyCurrent)
            {
                RecordCopyTextureToResolve(pFinalWorkBuffer, m_gbuffer.Get(GBufferTextureType::GBufferThisFrameColor));
            }
            else if (debugCopyHistory)
            {
                RecordCopyTextureToResolve(pFinalWorkBuffer, m_gbuffer.Get(GBufferTextureType::GBufferLastFrameColor));
            }
            else
            {
                // Resolve is only a TAA target here. DLSS manages its own output transition/write path.
                RecordResolveToGeneral(pFinalWorkBuffer);
                RenderPassGroup(PassType::TAA, mainPassData, ctx, pFinalWorkBuffer);
                RecordResolveToRead(pFinalWorkBuffer);
            }
        }

        RenderPassGroup(PassType::DLSS, mainPassData, ctx, pFinalWorkBuffer);

        RenderPassGroup(PassType::Composite, mainPassData, ctx, pFinalWorkBuffer);
        RenderPassGroup(PassType::UI, mainPassData, ctx, pFinalWorkBuffer);

        RecordSwapchainToPresent(pFinalWorkBuffer);

        pFinalWorkBuffer->AddSignalSemaphore(&ctx.pPresentLayoutTransitionSignalSemaphore);
        pFinalWorkBuffer->AddTimelineSignal(&ctx.frameTimeline, graphicsTimelineValue);
        pFinalWorkBuffer->SetSignalStages(SyncStages::COLOR_ATTACHMENT_OUTPUT);
        pFinalWorkBuffer->Bake();
        g_pQueueHandler->SubmitCommandBufferThisFrame({pFinalWorkBuffer, QueueType::Graphics, ctx.currentFrame});
    }

    ctx.nextComputeTimelineValue = sssSignalValue;
    ctx.nextTimelineValue = graphicsTimelineValue;

    g_pQueueHandler->FlushGraphicsComputeBuffers();
}

void PassManager::UpdateGBufferUBO(const MainPassData& data)
{
    const auto& temporal = data.temporalResources;
    UBO::GBufferPostProcessUBO gbufferUBO{};
    gbufferUBO.gbufferAlbedoIdx = m_gbuffer.GetHandle(GBufferTextureType::GBufferAlbedo);
    gbufferUBO.gbufferNormalIdx = m_gbuffer.GetHandle(GBufferTextureType::GBufferNormal);
    gbufferUBO.gbufferTexCoordMatIdx = m_gbuffer.GetHandle(GBufferTextureType::TexCoordMatData);
    gbufferUBO.gbufferDebugIdx = m_gbuffer.GetHandle(GBufferTextureType::GBufferDebug);
    gbufferUBO.gbufferVelocityIdx = m_gbuffer.GetHandle(GBufferTextureType::GBufferVelocity);
    gbufferUBO.lastFrameVelocityIdx = m_gbuffer.GetHandle(GBufferTextureType::GBufferLastFrameVelocity);
    gbufferUBO.depthBufferIdx = temporal.currentDepthHandle;
    gbufferUBO.lastFrameColorBufferIdx = temporal.historyColorHandle;
    gbufferUBO.thisFrameColorBufferIdx = temporal.currentColorHandle;
    gbufferUBO.lastFrameDepthIdx = temporal.historyDepthHandle;
    gbufferUBO.gbufferResolveIdx = temporal.resolveHandle;
    gbufferUBO.rtDebugViewIdx = data.rtDebugTextureHandle;

    memcpy(m_frameResourceManager.GetMappedGBufferPostProcessUBO(),
           &gbufferUBO,
           sizeof(UBO::GBufferPostProcessUBO));
}

void PassManager::RecordInitialLayoutTransitions(CommandBuffer* pCmdBuffer,
                                                 const stltype::fixed_vector<const Texture*, 16>& allGbufferAndSwapchain)
{
    ImageLayoutTransitionCmd colorCmd(stltype::vector<const Texture*>(allGbufferAndSwapchain.begin(), allGbufferAndSwapchain.end()));
    colorCmd.oldLayout = ImageLayout::UNDEFINED;
    colorCmd.newLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(colorCmd, ImageLayout::UNDEFINED, ImageLayout::COLOR_ATTACHMENT_OPTIMAL);
    pCmdBuffer->RecordCommand(colorCmd);

    stltype::fixed_vector<const Texture*, 2> depthTextures;
    depthTextures.push_back(m_globalRendererAttachments.depthAttachment.GetTexture());
    if (m_globalRendererAttachments.directionalLightShadowMap.pTexture)
    {
        depthTextures.push_back(m_globalRendererAttachments.directionalLightShadowMap.pTexture);
    }
    
    ImageLayoutTransitionCmd depthCmd(stltype::vector<const Texture*>(depthTextures.begin(), depthTextures.end()));
    depthCmd.oldLayout = ImageLayout::UNDEFINED;
    depthCmd.newLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(depthCmd, ImageLayout::UNDEFINED, ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    pCmdBuffer->RecordCommand(depthCmd);
}

void PassManager::RecordGBufferToShaderRead(CommandBuffer* pCmdBuffer,
                                            const stltype::fixed_vector<const Texture*, 8>& gbufferTextures)
{
    ImageLayoutTransitionCmd colorCmd(stltype::vector<const Texture*>(gbufferTextures.begin(), gbufferTextures.end()));
    colorCmd.oldLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    colorCmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(colorCmd, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(colorCmd);

    if (const auto* pShadowMap = m_globalRendererAttachments.directionalLightShadowMap.pTexture)
    {
        ImageLayoutTransitionCmd shadowCmd(pShadowMap);
        shadowCmd.oldLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        shadowCmd.newLayout = ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        VkTextureManager::SetLayoutBarrierMasks(shadowCmd, ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        pCmdBuffer->RecordCommand(shadowCmd);
    }

}

void PassManager::RecordVelocityClear(CommandBuffer* pCmdBuffer)
{
    RecordClearColorTexture(pCmdBuffer,
                            m_gbuffer.Get(GBufferTextureType::GBufferVelocity),
                            ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
                            ImageLayout::COLOR_ATTACHMENT_OPTIMAL);
}

void PassManager::RecordDepthToReadOnly(CommandBuffer* pCmdBuffer)
{
    ImageLayoutTransitionCmd depthCmd(m_globalRendererAttachments.depthAttachment.GetTexture());
    depthCmd.oldLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthCmd.newLayout = ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(depthCmd, ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(depthCmd);
}

void PassManager::RecordSSSOutputToGeneral(CommandBuffer* pCmdBuffer)
{
    if (!m_pScreenSpaceShadowTexture) return;
    ImageLayoutTransitionCmd cmd(m_pScreenSpaceShadowTexture);
    cmd.oldLayout = ImageLayout::UNDEFINED;
    cmd.newLayout = ImageLayout::GENERAL;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::UNDEFINED, ImageLayout::GENERAL);
    pCmdBuffer->RecordCommand(cmd);
}

void PassManager::RecordSSSOutputToShaderRead(CommandBuffer* pCmdBuffer)
{
    if (!m_pScreenSpaceShadowTexture) return;
    ImageLayoutTransitionCmd cmd(m_pScreenSpaceShadowTexture);
    cmd.oldLayout = ImageLayout::GENERAL;
    cmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::GENERAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    cmd.dstStage = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    cmd.dstAccessMask = 0;
    pCmdBuffer->RecordCommand(cmd);
}

void PassManager::RecordDLSSExposureUpdate(CommandBuffer* pCmdBuffer)
{
    if (!m_pDLSSExposureTexture)
        return;

    const float exposureValue = g_pApplicationState->GetCurrentApplicationState().renderState.exposure;
    m_dlssExposureStagingBuffer.CopyToMapped(&exposureValue, sizeof(exposureValue));

    ImageLayoutTransitionCmd exposureToTransfer(m_pDLSSExposureTexture);
    exposureToTransfer.oldLayout =
        m_dlssExposureTextureInitialized ? ImageLayout::SHADER_READ_ONLY_OPTIMAL : ImageLayout::UNDEFINED;
    exposureToTransfer.newLayout = ImageLayout::TRANSFER_DST_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(exposureToTransfer,
                                            exposureToTransfer.oldLayout,
                                            ImageLayout::TRANSFER_DST_OPTIMAL);
    if (m_dlssExposureTextureInitialized)
        exposureToTransfer.srcStage = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    pCmdBuffer->RecordCommand(exposureToTransfer);

    ImageBufferCopyCmd copyExposure(&m_dlssExposureStagingBuffer, m_pDLSSExposureTexture);
    copyExposure.imageExtent = {1, 1, 1};
    copyExposure.aspectFlagBits = VK_IMAGE_ASPECT_COLOR_BIT;
    pCmdBuffer->RecordCommand(copyExposure);

    ImageLayoutTransitionCmd exposureToRead(m_pDLSSExposureTexture);
    exposureToRead.oldLayout = ImageLayout::TRANSFER_DST_OPTIMAL;
    exposureToRead.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(exposureToRead,
                                            ImageLayout::TRANSFER_DST_OPTIMAL,
                                            ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    exposureToRead.dstStage = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    pCmdBuffer->RecordCommand(exposureToRead);

    m_dlssExposureTextureInitialized = true;
}

void PassManager::RecordThisFrameColorToRead(CommandBuffer* pCmdBuffer)
{
    ImageLayoutTransitionCmd cmd(m_gbuffer.Get(GBufferTextureType::GBufferThisFrameColor));
    cmd.oldLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    cmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(cmd);
}

void PassManager::RecordResolveToGeneral(CommandBuffer* pCmdBuffer)
{
    Texture* pResolve = m_gbuffer.Get(GBufferTextureType::GBufferResolve);
    const ImageLayout oldLayout =
        m_temporalResolveWrites < 2 ? ImageLayout::UNDEFINED : ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    RecordClearColorTexture(pCmdBuffer, pResolve, oldLayout, ImageLayout::GENERAL);
}

void PassManager::RecordResolveToRead(CommandBuffer* pCmdBuffer)
{
    ImageLayoutTransitionCmd cmd(m_gbuffer.Get(GBufferTextureType::GBufferResolve));
    cmd.oldLayout = ImageLayout::GENERAL;
    cmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::GENERAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(cmd);
    ++m_temporalResolveWrites;
}

void PassManager::RecordCopyTextureToResolve(CommandBuffer* pCmdBuffer, Texture* pSourceTexture)
{
    Texture* pResolve = m_gbuffer.Get(GBufferTextureType::GBufferResolve);
    const ImageLayout oldResolveLayout =
        m_temporalResolveWrites < 2 ? ImageLayout::UNDEFINED : ImageLayout::SHADER_READ_ONLY_OPTIMAL;

    ImageLayoutTransitionCmd sourceToCopy(pSourceTexture);
    sourceToCopy.oldLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    sourceToCopy.newLayout = ImageLayout::TRANSFER_SRC_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(
        sourceToCopy, ImageLayout::SHADER_READ_ONLY_OPTIMAL, ImageLayout::TRANSFER_SRC_OPTIMAL);
    pCmdBuffer->RecordCommand(sourceToCopy);

    ImageLayoutTransitionCmd resolveToCopy(pResolve);
    resolveToCopy.oldLayout = oldResolveLayout;
    resolveToCopy.newLayout = ImageLayout::TRANSFER_DST_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(resolveToCopy, oldResolveLayout, ImageLayout::TRANSFER_DST_OPTIMAL);
    pCmdBuffer->RecordCommand(resolveToCopy);

    ImageToImageCopyCmd copyCmd(pSourceTexture, pResolve);
    pCmdBuffer->RecordCommand(copyCmd);

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

    ++m_temporalResolveWrites;
}

void PassManager::RecordClearColorTexture(CommandBuffer* pCmdBuffer,
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

void PassManager::RecordSwapchainToPresent(CommandBuffer* pCmdBuffer)
{
    ImageLayoutTransitionCmd cmd(m_frameResourceManager.GetFrameRendererContext(m_currentSwapChainIdx).pCurrentSwapchainTexture);
    cmd.oldLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    cmd.newLayout = ImageLayout::PRESENT_SRC_KHR;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, ImageLayout::PRESENT_SRC_KHR);
    pCmdBuffer->RecordCommand(cmd);
}

void PassManager::Init()
{
    InitResourceManagerAndCallbacks();
    CreateUBOsAndMap();
    InitFrameContexts();
    InitPassesAndImGui();
    m_gpuTimingQuery.Init(128);
    for (auto& [type, passes] : m_passes)
    {
        for (auto& pPass : passes)
            pPass->SetTimingQuery(&m_gpuTimingQuery);
    }
}

void PassManager::SyncRenderStateFromAppState()
{
    const auto& appRenderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    if (m_renderState.swapchainResolution.x <= 0.0f || m_renderState.swapchainResolution.y <= 0.0f)
        m_renderState.swapchainResolution = FrameGlobals::GetSwapChainExtent();
    m_renderState.renderResolution =
        CalculateRenderResolution(m_renderState.swapchainResolution, appRenderState.upscalingPercentage);
    g_pApplicationState->RegisterUpdateFunction(
        [renderResolution = m_renderState.renderResolution, swapchainResolution = m_renderState.swapchainResolution](ApplicationState& state)
        {
            state.renderState.renderResolution = renderResolution;
            state.renderState.swapchainResolution = swapchainResolution;
        });
}

void PassManager::HandlePendingSwapchainRecreatedEvent()
{
    SwapchainRecreatedEventData recreatedEvent{};
    {
        SimpleScopedGuard<CustomMutex> lock(m_swapchainEventMutex);
        if (!m_hasPendingSwapchainRecreatedEvent)
            return;
        recreatedEvent = m_pendingSwapchainRecreatedEvent;
        m_hasPendingSwapchainRecreatedEvent = false;
    }

    m_renderState.swapchainResolution = recreatedEvent.swapchainResolution;
    m_renderState.renderResolution = recreatedEvent.renderResolution;
    m_renderState.recreatedThisFrame = recreatedEvent.swapchainWasRecreated;
    RecreateAllRenderResources();
}

void PassManager::ExecutePasses(u32 frameIdx)
{
    auto& ctx = m_frameResourceManager.GetFrameRendererContext(m_currentSwapChainIdx);
    auto& mainPassData = m_mainPassData.at(m_currentSwapChainIdx);
    auto& imageAvailableSemaphore = m_imageAvailableSemaphores.at(frameIdx);

    ctx.imageIdx = m_currentSwapChainIdx;
    ctx.currentFrame = frameIdx;
    ctx.pCurrentSwapchainTexture = Texture::Cast(&g_pTexManager->GetSwapChainTextures().at(m_currentSwapChainIdx));

    g_pQueueHandler->DispatchAllRequests();
    m_rtSceneManager.Update(frameIdx, m_currentSwapChainIdx, m_frameResourceManager);
    PrepareMainPassDataForFrame(mainPassData, ctx, frameIdx);
    RenderAllPassGroups(mainPassData, ctx, imageAvailableSemaphore);

    AsyncQueueHandler::PresentRequest presentRequest{.pWaitSemaphore = ctx.renderingFinishedSemaphore,
                                                     .swapChainImageIdx = m_currentSwapChainIdx};

    g_pQueueHandler->SubmitSwapchainPresentRequestForThisFrame(presentRequest);
}

void PassManager::ReadAndPublishTimingResults(u32 frameIdx)
{
    if (!m_gpuTimingQuery.IsEnabled()) return;
    m_gpuTimingQuery.ReadResults(frameIdx);

    const auto& results = m_gpuTimingQuery.GetResults();
    f32 totalTime = m_gpuTimingQuery.GetTotalGPUTimeMs();

    stltype::vector<PassTimingStat> passTimings;
    passTimings.reserve(results.size());
    for (const auto& r : results)
        passTimings.push_back({r.passName, r.gpuTimeMs, r.startMs, r.endMs, r.queueFamilyIndex, r.wasRun});

    u64 totalVram = 0, usedVram = 0;
    g_pGPUMemoryManager->GetVramStats(totalVram, usedVram);

    g_pApplicationState->RegisterUpdateFunction(
        [passTimings = std::move(passTimings), totalTime, totalVram, usedVram](ApplicationState& state)
        {
            state.renderState.passTimings = std::move(passTimings);
            state.renderState.totalGPUTimeMs = totalTime;
            state.renderState.totalVramBytes = totalVram;
            state.renderState.usedVramBytes = usedVram;
        });
}

PassManager::~PassManager() { m_gpuTimingQuery.Destroy(); }

void PassManager::AddPass(PassType type, stltype::unique_ptr<ConvolutionRenderPass>&& pass)
{
    m_passes[type].push_back(std::move(pass));
}

void PassManager::TransferPassData(const PassGeometryData& passData, u32 frameIdx)
{
}

void PassManager::SetEntityMeshDataForFrame(EntityMeshDataMap&& data, u32 frameIdx) { m_frameResourceManager.SetEntityMeshDataForFrame(std::move(data), frameIdx); }
void PassManager::SetEntityTransformDataForFrame(TransformSystemData&& data, u32 frameIdx) { m_frameResourceManager.SetEntityTransformDataForFrame(std::move(data), frameIdx); }
void PassManager::SetLightDataForFrame(PointLightVector&& data, DirLightVector&& dirLights, u32 frameIdx) { m_frameResourceManager.SetLightDataForFrame(std::move(data), std::move(dirLights), frameIdx); }
void PassManager::SetLightDeltaForFrame(stltype::vector<LightDeltaUpdate>&& updates, bool dirLightDirty, const DirectionalRenderLight& dirLight, u32 frameIdx) { m_frameResourceManager.SetLightDeltaForFrame(std::move(updates), dirLightDirty, dirLight, frameIdx); }
void PassManager::SetSharedData(RenderView&& mainView, u32 frameIdx) { m_frameResourceManager.SetSharedData(std::move(mainView), frameIdx); }
void PassManager::PreProcessDataForCurrentFrame(u32 frameIdx, u64 jitterFrameNumber)
{
    m_renderState.recreatedThisFrame = false;
    g_pApplicationState->RegisterUpdateFunction(
        [](ApplicationState& state) { state.renderState.renderTargetsRecreatedThisFrame = false; });
    HandlePendingSwapchainRecreatedEvent();

    // Rotate per-frame history targets before building any frame descriptors/state.
    m_gbuffer.FlipHistoryBuffers();
    stltype::swap(m_pDepthTexture, m_pLastFrameDepthTexture);
    stltype::swap(m_depthBindlessHandle, m_lastFrameDepthBindlessHandle);
    m_globalRendererAttachments.depthAttachment.SetTexture(m_pDepthTexture);
    for (auto& mainPassData : m_mainPassData)
    {
        UpdateTemporalResources(mainPassData);
    }

    m_frameResourceManager.PreProcessDataForCurrentFrame(frameIdx, jitterFrameNumber, m_currentSwapChainIdx, this);
}

void PassManager::RecreateGbuffers(const mathstl::Vector2& resolution)
{
    m_renderState.recreatedThisFrame = true;
    m_temporalResolveWrites = 0;
    g_pApplicationState->RegisterUpdateFunction(
        [](ApplicationState& state) { state.renderState.renderTargetsRecreatedThisFrame = true; });
    auto& gbuffer = m_gbuffer;
    DynamicTextureRequest baseRequest{};
    baseRequest.extents = DirectX::XMUINT3(resolution.x, resolution.y, 1);
    baseRequest.usage = Usage::GBuffer | Usage::TransferSrc | Usage::TransferDst;
    baseRequest.isPersistent = true;

    struct GBufferTexDef {
        GBufferTextureType type;
        const char* name;
        Usage extraUsage = Usage::None;
        bool nearest = true;
    };
    
    static const GBufferTexDef defs[] = {
        { GBufferTextureType::GBufferAlbedo, "GBuffer Albedo" },
        { GBufferTextureType::GBufferNormal, "GBuffer Normal" },
        { GBufferTextureType::TexCoordMatData, "GBuffer UV Material Data" },
        { GBufferTextureType::GBufferDebug, "GBuffer Debug" },
        { GBufferTextureType::GBufferVelocity, "GBuffer Velocity", Usage::None, false },
        { GBufferTextureType::GBufferLastFrameVelocity, "GBuffer Last Frame Velocity", Usage::None, false },
        { GBufferTextureType::GBufferLastFrameColor, "GBuffer Last Frame Color", Usage::Storage, false },
        { GBufferTextureType::GBufferThisFrameColor, "GBuffer This Frame Color", Usage::Storage, false },
        { GBufferTextureType::GBufferResolve, "GBuffer Resolve", Usage::Storage | Usage::Sampled, false }
    };

    for (const auto& def : defs)
    {
        DynamicTextureRequest req = baseRequest;
        req.format = gbuffer.GetFormat(def.type);
        req.handle = g_pTexManager->GenerateHandle();
        req.usage |= def.extraUsage;
        req.AddName(def.name);
        if (def.nearest) {
            req.samplerInfo.minFilter = TextureFilter::NEAREST;
            req.samplerInfo.magFilter = TextureFilter::NEAREST;
        }
        
        gbuffer.Set(def.type, static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(req)));
        gbuffer.SetHandle(def.type, g_pTexManager->MakeTextureBindless(req.handle, true));
    }

    DynamicTextureRequest sssRequest = baseRequest;
    sssRequest.format = TexFormat::R8_UNORM;
    sssRequest.handle = g_pTexManager->GenerateHandle();
    sssRequest.AddName("Screen Space Shadows");
    sssRequest.usage = Usage::Storage | Usage::Sampled;
    if (m_pScreenSpaceShadowTexture) g_pTexManager->FreeTexture(m_screenSpaceShadowsTextureHandle);
    m_pScreenSpaceShadowTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(sssRequest));
    m_screenSpaceShadowsTextureHandle = sssRequest.handle;
    m_screenSpaceShadowBindlessHandle = g_pTexManager->MakeTextureBindless(sssRequest.handle, true);
    m_globalRendererAttachments.pScreenSpaceShadowTexture = m_pScreenSpaceShadowTexture;

    DynamicTextureRequest smaaEdgeReq = baseRequest;
    smaaEdgeReq.format = TexFormat::R8G8_UNORM;
    smaaEdgeReq.handle = g_pTexManager->GenerateHandle();
    smaaEdgeReq.AddName("SMAA Edges");
    smaaEdgeReq.usage = Usage::GBuffer | Usage::Storage | Usage::Sampled;
    if (m_pSMAAEdgesTexture) g_pTexManager->FreeTexture(m_smaaEdgesTextureHandle);
    m_pSMAAEdgesTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(smaaEdgeReq));
    m_smaaEdgesTextureHandle = smaaEdgeReq.handle;
    m_smaaEdgesBindlessHandle = g_pTexManager->MakeTextureBindless(smaaEdgeReq.handle, true);

    DynamicTextureRequest smaaBlendReq = baseRequest;
    smaaBlendReq.format = TexFormat::R8G8B8A8_UNORM;
    smaaBlendReq.handle = g_pTexManager->GenerateHandle();
    smaaBlendReq.AddName("SMAA Blend Weights");
    smaaBlendReq.usage = Usage::GBuffer | Usage::Storage | Usage::Sampled;
    if (m_pSMAABlendTexture) g_pTexManager->FreeTexture(m_smaaBlendTextureHandle);
    m_pSMAABlendTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(smaaBlendReq));
    m_smaaBlendTextureHandle = smaaBlendReq.handle;
    m_smaaBlendBindlessHandle = g_pTexManager->MakeTextureBindless(smaaBlendReq.handle, true);

    m_globalRendererAttachments.pSMAAEdgesTexture = m_pSMAAEdgesTexture;
    m_globalRendererAttachments.pSMAABlendTexture = m_pSMAABlendTexture;

    if (m_dlssExposureTextureHandle != 0)
    {
        g_pTexManager->FreeTexture(m_dlssExposureTextureHandle);
        m_dlssExposureTextureHandle = 0;
        m_pDLSSExposureTexture = nullptr;
    }

    if (Nvidia::StreamlineManager::IsDLSSSupported())
    {
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
        m_dlssExposureTextureInitialized = false;
        m_dlssExposureStagingBuffer.EnsureCapacity(sizeof(float));
    }

    for (auto& passData : m_mainPassData) {
        passData.pScreenSpaceShadowTexture = m_pScreenSpaceShadowTexture;
        passData.screenSpaceShadows = m_screenSpaceShadowBindlessHandle;
        passData.pSMAAEdgesTexture = m_pSMAAEdgesTexture;
        passData.pSMAABlendTexture = m_pSMAABlendTexture;
        passData.smaaEdges = m_smaaEdgesBindlessHandle;
        passData.smaaBlend = m_smaaBlendBindlessHandle;
        UpdateTemporalResources(passData);
    }

    UpdateGBufferUBO(m_mainPassData[m_currentSwapChainIdx]);
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; i++)
        m_frameResourceManager.GetFrameRendererContext(i).gbufferPostProcessDescriptor->WriteBufferUpdate(m_frameResourceManager.GetGBufferPostProcessUBO(), s_globalGbufferPostProcessUBOSlot);
}

bool PassManager::BlockUntilPassesFinished(u32 frameIdx)
{
    auto& fence = m_imageAvailableFences.at(frameIdx);
    auto& sem = m_imageAvailableSemaphores.at(frameIdx);
    fence.Reset();
    if (!SRF::QueryImageForPresentationFromMainSwapchain<RenderAPI>(sem, fence, m_currentSwapChainIdx)) return false;
    fence.WaitFor();
    fence.Reset();
    g_pQueueHandler->WaitForFences(frameIdx);
    return true;
}

void PassManager::RebuildPipelinesForAllPasses()
{
    if (!g_pShaderManager->ReloadAllShaders()) return;
    for (auto& [type, passes] : m_passes) {
        for (auto& pass : passes) pass->BuildPipelines();
    }
}

void PassManager::PreProcessMeshData(const stltype::vector<PassMeshData>& meshes, u32 lastFrame, u32 curFrame)
{
    auto& lastFrameCtx = m_frameResourceManager.GetFrameRendererContext(lastFrame);
    lastFrameCtx.pResourceManager = &m_resourceManager;
    for (auto& [type, passes] : m_passes) {
        for (auto& pass : passes) {
            pass->RebuildInternalData(meshes, lastFrameCtx, curFrame);
            pass->NameResources(pass->GetName());
        }
    }
}

void PassManager::RecreateShadowMaps(u32 cascades, const mathstl::Vector2& extents)
{
    m_renderState.recreatedThisFrame = true;
    g_pApplicationState->RegisterUpdateFunction(
        [](ApplicationState& state) { state.renderState.renderTargetsRecreatedThisFrame = true; });
    auto& csm = m_globalRendererAttachments.directionalLightShadowMap;
    const TextureHandle oldHandle = csm.handle;
    auto oldCascadeViews = stltype::move(csm.cascadeViews);
    auto oldImGuiIDs = stltype::move(m_csmCascadeImGuiIDs);
    if (oldHandle != 0 || !oldCascadeViews.empty() || !oldImGuiIDs.empty())
    {
        g_pDeleteQueue->RegisterDeleteForNextFrame(
            [oldHandle, oldCascadeViews = stltype::move(oldCascadeViews), oldImGuiIDs = stltype::move(oldImGuiIDs)]() mutable
            {
                for (const auto id : oldImGuiIDs)
                {
                    if (id != 0)
                    {
                        ImGui_ImplVulkan_RemoveTexture(reinterpret_cast<VkDescriptorSet>(id));
                    }
                }

                for (const auto view : oldCascadeViews)
                {
                    if (view != VK_NULL_HANDLE)
                    {
                        vkDestroyImageView(VkGlobals::GetLogicalDevice(), view, nullptr);
                    }
                }

                if (oldHandle != 0)
                {
                    g_pTexManager->FreeTexture(oldHandle);
                }
            });
    }

    csm.cascades = cascades;
    csm.format = DEPTH_BUFFER_FORMAT;
    DynamicTextureRequest req{};
    req.AddName("Directional Light CSM");
    req.isPersistent = true;
    csm.handle = req.handle = g_pTexManager->GenerateHandle();
    req.extents = DirectX::XMUINT3(extents.x, extents.y, cascades);
    req.format = csm.format;
    req.usage = Usage::ShadowMap;
    req.samplerInfo.wrapU = req.samplerInfo.wrapV = req.samplerInfo.wrapW = TextureWrapMode::CLAMP_TO_BORDER;
    csm.pTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(req));
    csm.bindlessHandle = g_pTexManager->MakeTextureBindless(req.handle, true);

    csm.cascadeViews.resize(cascades, VK_NULL_HANDLE);

    for (u32 i = 0; i < cascades; ++i) {
        VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        viewInfo.image = csm.pTexture->GetImage();
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = Conv(csm.format);
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = i;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(VkGlobals::GetLogicalDevice(), &viewInfo, nullptr, &csm.cascadeViews[i]);
    }

    UBO::ShadowMapUBO shadowMapUBO{};
    shadowMapUBO.directionalShadowMapIdx = csm.bindlessHandle;
    std::memcpy(m_frameResourceManager.GetMappedShadowMapUBO(), &shadowMapUBO, sizeof(shadowMapUBO));
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; i++)
        m_frameResourceManager.GetFrameRendererContext(i).gbufferPostProcessDescriptor->WriteBufferUpdate(m_frameResourceManager.GetShadowMapUBO(), s_shadowmapUBOBindingSlot);

    auto& shadowPasses = m_passes.at(PassType::Shadow);
    for (auto& pass : shadowPasses) if (auto* cp = dynamic_cast<CSMPass*>(pass.get())) cp->SetCascadeCount(cascades);
}

void PassManager::RegisterImGuiTextures()
{
    m_csmCascadeImGuiIDs.clear();
    const auto& csm = m_globalRendererAttachments.directionalLightShadowMap;
    if (!csm.pTexture || csm.cascadeViews.empty()) {
        g_pApplicationState->RegisterUpdateFunction([](ApplicationState& state) { state.renderState.csmCascadeImGuiIDs.clear(); });
        return;
    }
    for (auto view : csm.cascadeViews) {
        if (view == VK_NULL_HANDLE) continue;
        m_csmCascadeImGuiIDs.push_back(reinterpret_cast<u64>(ImGui_ImplVulkan_AddTexture(csm.pTexture->GetSampler(), view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)));
    }
    g_pApplicationState->RegisterUpdateFunction([ids = m_csmCascadeImGuiIDs](ApplicationState& state) { state.renderState.csmCascadeImGuiIDs = ids; });
}

void PassManager::UpdateImGuiGbufferTextures()
{
    m_gbufferImGuiIDs.clear();
    auto addTex = [&](GBufferTextureType type) {
        auto* t = m_gbuffer.Get(type);
        return reinterpret_cast<u64>(ImGui_ImplVulkan_AddTexture(t->GetSampler(), t->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    };
    m_gbufferImGuiIDs.push_back(addTex(GBufferTextureType::GBufferNormal));
    m_gbufferImGuiIDs.push_back(addTex(GBufferTextureType::GBufferAlbedo));
    m_gbufferImGuiIDs.push_back(reinterpret_cast<u64>(ImGui_ImplVulkan_AddTexture(m_pScreenSpaceShadowTexture->GetSampler(), m_pScreenSpaceShadowTexture->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)));

    Texture* pVelocityA = m_gbuffer.Get(GBufferTextureType::GBufferVelocity);
    u64 velocityIdA = addTex(GBufferTextureType::GBufferVelocity);
    u64 velocityIdB = addTex(GBufferTextureType::GBufferLastFrameVelocity);
    m_gbufferImGuiIDs.push_back(velocityIdA);

    m_gbufferImGuiIDs.push_back(addTex(GBufferTextureType::GBufferThisFrameColor));
    
    Texture* pHistoryColorA = m_gbuffer.Get(GBufferTextureType::GBufferLastFrameColor);
    u64 historyColorIdA = addTex(GBufferTextureType::GBufferLastFrameColor);
    u64 historyColorIdB = addTex(GBufferTextureType::GBufferResolve);
    m_gbufferImGuiIDs.push_back(historyColorIdA);
    m_gbufferImGuiIDs.push_back(historyColorIdB);

    g_pApplicationState->RegisterUpdateFunction(
        [this, pVelocityA, velocityIdA, velocityIdB, pHistoryColorA, historyColorIdA, historyColorIdB](auto& state) {
        state.renderState.csmCascadeImGuiIDs = m_csmCascadeImGuiIDs;
        state.renderState.gbufferImGuiIDs = m_gbufferImGuiIDs;
        const bool velocitySwapped = m_gbuffer.Get(GBufferTextureType::GBufferVelocity) != pVelocityA;
        state.renderState.gbufferImGuiIDs[3] = velocitySwapped ? velocityIdB : velocityIdA;

        const bool colorSwapped = m_gbuffer.Get(GBufferTextureType::GBufferLastFrameColor) != pHistoryColorA;
        state.renderState.gbufferImGuiIDs[5] = colorSwapped ? historyColorIdB : historyColorIdA;
        state.renderState.gbufferImGuiIDs[6] = colorSwapped ? historyColorIdA : historyColorIdB;
    });
}
