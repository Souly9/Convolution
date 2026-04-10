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
#include "AA/SMAAPass.h"
#include "Compositing/LightingPass.h"
#include "Core/Rendering/Core/ProfilingUtils.h"
#include "vulkan/vulkan_core.h"
#include <cstring>
#include <imgui/backends/imgui_impl_vulkan.h>

using namespace RenderPasses;

// Helper implementations to break up large functions
void PassManager::InitResourceManagerAndCallbacks()
{
    m_resourceManager.Init();
    g_pEventSystem->AddSceneLoadedEventCallback(
        [this](const SceneLoadedEventData&) { m_resourceManager.UploadSceneGeometry(g_pMeshManager->GetMeshes()); });
    g_pEventSystem->AddShaderHotReloadEventCallback([this](const auto&) { RebuildPipelinesForAllPasses(); });

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
    AddPass(PassType::TAA, stltype::make_unique<RenderPasses::TAAPass>());
    AddPass(PassType::SMAA, stltype::make_unique<RenderPasses::SMAAPass>());
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
    RecreateAllRenderResources();
}

void PassManager::CreateDepthAttachment()
{
    DepthBufferAttachmentInfo depthAttachmentInfo{};
    depthAttachmentInfo.format = DEPTH_BUFFER_FORMAT;

    DynamicTextureRequest depthRequest{};
    depthRequest.handle = g_pTexManager->GenerateHandle();
    depthRequest.extents =
        DirectX::XMUINT3(FrameGlobals::GetSwapChainExtent().x, FrameGlobals::GetSwapChainExtent().y, 1);
    depthRequest.format = DEPTH_BUFFER_FORMAT;
    depthRequest.usage = Usage::Sampled | Usage::DepthAttachment | Usage::TransferDst | Usage::TransferSrc;
    depthRequest.isPersistent = true;
    depthRequest.AddName("Main Depth Buffer");

    m_pDepthTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(depthRequest));
    m_depthBindlessHandle = g_pTexManager->MakeTextureBindless(depthRequest.handle, depthRequest.isPersistent);

    depthRequest.handle = g_pTexManager->GenerateHandle();
    depthRequest.AddName("Last Frame Depth Buffer");
    
    m_pLastFrameDepthTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(depthRequest));
    m_lastFrameDepthBindlessHandle = g_pTexManager->MakeTextureBindless(depthRequest.handle, depthRequest.isPersistent);

    m_globalRendererAttachments.depthAttachment = DepthAttachment::Create(depthAttachmentInfo, m_pDepthTexture);
}

void PassManager::RecreateAllRenderResources()
{
    ScopedZone("PassManager::RecreateAllRenderResources");

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
    RecreateGbuffers(mathstl::Vector2(FrameGlobals::GetSwapChainExtent().x, FrameGlobals::GetSwapChainExtent().y));

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

        mainPassData.depthBufferBindlessHandle = m_depthBindlessHandle;
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
    mainPassData.directionalLightShadowMap = m_globalRendererAttachments.directionalLightShadowMap;
    mainPassData.cascades = m_frameResourceManager.GetShadowMapState().cascadeCount;
    mainPassData.depthBufferBindlessHandle = m_depthBindlessHandle;
    mainPassData.pMainDepthTexture = m_pDepthTexture;
    mainPassData.pLastFrameDepthTexture = m_pLastFrameDepthTexture;

    mainPassData.bufferDescriptors[UBO::DescriptorContentsType::GlobalInstanceData] =
        m_resourceManager.GetInstanceSSBODescriptorSet(frameIdx);
    mainPassData.bufferDescriptors[UBO::DescriptorContentsType::BindlessTextureArray] =
        g_pTexManager->GetBindlessDescriptorSet();
    mainPassData.bufferDescriptors[UBO::DescriptorContentsType::ClusterGrid] = ctx.clusterGridDescriptor;

    mainPassData.pSMAAEdgesTexture = m_pSMAAEdgesTexture;
    mainPassData.pSMAABlendTexture = m_pSMAABlendTexture;
    mainPassData.smaaEdges = m_smaaEdgesBindlessHandle;
    mainPassData.smaaBlend = m_smaaBlendBindlessHandle;
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
    const auto& passes = m_passes.at(groupType);
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

    // Flip gbuffer history buffers before starting the frame
    m_gbuffer.FlipHistoryBuffers();

    UpdateGBufferUBO();

    if (m_gpuTimingQuery.IsEnabled())
    {
        m_gpuTimingQuery.ResetQueries(ctx.currentFrame);
    }

    stltype::fixed_vector<const Texture*, 8> gbufferTextures;
    m_gbuffer.GetGeometryOutputTextures(gbufferTextures);

    stltype::fixed_vector<const Texture*, 16> allColorTextures;
    allColorTextures.assign(gbufferTextures.begin(), gbufferTextures.end());
    allColorTextures.push_back(m_gbuffer.Get(GBufferTextureType::GBufferThisFrameColor));
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
    if (!pendingFlips.empty())
    {
        Profiling::StartScope(pDepthWorkBuffer, &m_gpuTimingQuery, m_instanceBufferUpdateTimingIndex, "Instance Buffer Updates");

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
        pMainGraphicsWorkBuffer->SetWaitStages(SyncStages::EARLY_FRAGMENT_TESTS);

        RecordDepthToAttachment(pMainGraphicsWorkBuffer);

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
        pFinalWorkBuffer->SetWaitStages(SyncStages::FRAGMENT_SHADER);

        RenderPassGroup(PassType::Lighting, mainPassData, ctx, pFinalWorkBuffer);

        // ThisFrameColor to SHADER_READ
        RecordThisFrameColorToRead(pFinalWorkBuffer);

        // Resolve to GENERAL for TAA
        RecordResolveToGeneral(pFinalWorkBuffer);

        RenderPassGroup(PassType::TAA, mainPassData, ctx, pFinalWorkBuffer);

        // We prepare Resolve for SHADER_READ since SMAA will sample it
        RecordResolveToRead(pFinalWorkBuffer);

        RenderPassGroup(PassType::SMAA, mainPassData, ctx, pFinalWorkBuffer);

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

void PassManager::UpdateGBufferUBO()
{
    UBO::GBufferPostProcessUBO gbufferUBO{};
    gbufferUBO.gbufferAlbedo = m_gbuffer.GetHandle(GBufferTextureType::GBufferAlbedo);
    gbufferUBO.gbufferNormal = m_gbuffer.GetHandle(GBufferTextureType::GBufferNormal);
    gbufferUBO.gbufferTexCoordMat = m_gbuffer.GetHandle(GBufferTextureType::TexCoordMatData);
    gbufferUBO.gbufferDebug = m_gbuffer.GetHandle(GBufferTextureType::GBufferDebug);
    gbufferUBO.gbufferVelocity = m_gbuffer.GetHandle(GBufferTextureType::GBufferVelocity);
    gbufferUBO.depthBufferIdx = m_depthBindlessHandle;
    gbufferUBO.lastFrameColorBufferIdx = m_gbuffer.GetHandle(GBufferTextureType::GBufferLastFrameColor);
    gbufferUBO.thisFrameColorBufferIdx = m_gbuffer.GetHandle(GBufferTextureType::GBufferThisFrameColor);
    gbufferUBO.lastFrameDepthIdx = m_lastFrameDepthBindlessHandle;
    gbufferUBO.gbufferResolve = m_gbuffer.GetHandle(GBufferTextureType::GBufferResolve);

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

    RecordDepthToReadOnly(pCmdBuffer);
}

void PassManager::RecordDepthToReadOnly(CommandBuffer* pCmdBuffer)
{
    ImageLayoutTransitionCmd depthCmd(m_globalRendererAttachments.depthAttachment.GetTexture());
    depthCmd.oldLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthCmd.newLayout = ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(depthCmd, ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(depthCmd);
}

void PassManager::RecordDepthToAttachment(CommandBuffer* pCmdBuffer)
{
    ImageLayoutTransitionCmd depthCmd(m_globalRendererAttachments.depthAttachment.GetTexture());
    depthCmd.oldLayout = ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depthCmd.newLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(depthCmd, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL, ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
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
    ImageLayoutTransitionCmd cmd(m_gbuffer.Get(GBufferTextureType::GBufferResolve));
    cmd.oldLayout = ImageLayout::UNDEFINED;
    cmd.newLayout = ImageLayout::GENERAL;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::UNDEFINED, ImageLayout::GENERAL);
    pCmdBuffer->RecordCommand(cmd);
}

void PassManager::RecordResolveToRead(CommandBuffer* pCmdBuffer)
{
    ImageLayoutTransitionCmd cmd(m_gbuffer.Get(GBufferTextureType::GBufferResolve));
    cmd.oldLayout = ImageLayout::GENERAL;
    cmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::GENERAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(cmd);
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

void PassManager::ExecutePasses(u32 frameIdx)
{

    stltype::swap(m_pDepthTexture, m_pLastFrameDepthTexture);
    stltype::swap(m_depthBindlessHandle, m_lastFrameDepthBindlessHandle);
    m_globalRendererAttachments.depthAttachment.SetTexture(m_pDepthTexture);
    auto& ctx = m_frameResourceManager.GetFrameRendererContext(m_currentSwapChainIdx);
    auto& mainPassData = m_mainPassData.at(m_currentSwapChainIdx);
    auto& imageAvailableSemaphore = m_imageAvailableSemaphores.at(frameIdx);

    ctx.imageIdx = m_currentSwapChainIdx;
    ctx.currentFrame = frameIdx;
    ctx.pCurrentSwapchainTexture = Texture::Cast(&g_pTexManager->GetSwapChainTextures().at(m_currentSwapChainIdx));

    g_pQueueHandler->DispatchAllRequests();
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
void PassManager::PreProcessDataForCurrentFrame(u32 frameIdx) { m_frameResourceManager.PreProcessDataForCurrentFrame(frameIdx, m_currentSwapChainIdx, this); }

void PassManager::RecreateGbuffers(const mathstl::Vector2& resolution)
{
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

    for (auto& passData : m_mainPassData) {
        passData.pScreenSpaceShadowTexture = m_pScreenSpaceShadowTexture;
        passData.screenSpaceShadows = m_screenSpaceShadowBindlessHandle;
        passData.pSMAAEdgesTexture = m_pSMAAEdgesTexture;
        passData.pSMAABlendTexture = m_pSMAABlendTexture;
        passData.smaaEdges = m_smaaEdgesBindlessHandle;
        passData.smaaBlend = m_smaaBlendBindlessHandle;
    }

    UpdateGBufferUBO();
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

    std::memcpy(m_frameResourceManager.GetMappedShadowMapUBO(), &csm.bindlessHandle, sizeof(BindlessTextureHandle));
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
    m_gbufferImGuiIDs.push_back(addTex(GBufferTextureType::GBufferVelocity));
    
    Texture *pTexA = m_gbuffer.Get(GBufferTextureType::GBufferLastFrameColor);
    u64 idA = addTex(GBufferTextureType::GBufferLastFrameColor);
    u64 idB = addTex(GBufferTextureType::GBufferThisFrameColor);
    m_gbufferImGuiIDs.push_back(idA);
    m_gbufferImGuiIDs.push_back(addTex(GBufferTextureType::GBufferResolve));

    g_pApplicationState->RegisterUpdateFunction([this, pTexA, idA, idB](auto& state) {
        state.renderState.csmCascadeImGuiIDs = m_csmCascadeImGuiIDs;
        state.renderState.gbufferImGuiIDs = m_gbufferImGuiIDs;
        state.renderState.gbufferImGuiIDs[4] = (m_gbuffer.Get(GBufferTextureType::GBufferLastFrameColor) == pTexA) ? idA : idB;
    });
}
