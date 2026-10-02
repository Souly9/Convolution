#include "PassManager.h"
#include "AA/SMAAPass.h"
#include "AA/TAAPass.h"
#include "ClusteredShading/ClusterDebugPass.h"
#include "ClusteredShading/ClusterGeneratorComputePass.h"
#include "ClusteredShading/LightGridComputePass.h"
#include "ClusteredShading/LightTransformComputePass.h"
#include "ClusteredShading/TileAssignmentComputePass.h"
#include "Compositing/CompositPass.h"
#include "Compositing/LightingPass.h"
#include "Core/Rendering/Core/AntiAliasing.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Global/State/States.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/GPUTimingQuery.h"
#include "Core/Rendering/Core/ProfilingUtils.h"
#include "Core/Rendering/Core/RenderGraph/RenderGraph.h"
#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/TracyManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/SceneGraph/Scene.h"
#include "DebugShapePass.h"
#include "ImGuiPass.h"
#include "PostProcess/BloomPass.h"
#include "PreProcess/DepthPrePass.h"
#include "RT/RTAOPass.h"
#include "RT/RTCompositePass.h"
#include "RT/RTDebugViewPass.h"
#include "RT/RTReflectionsPass.h"
#include "ScreenSpaceShadowPass.h"
#include "ShadowPass.h"
#include "StaticMeshPass.h"

using namespace RenderPasses;

namespace
{
AA::Support GetAASupport()
{
    AA::Support support{};
    support.dlss = g_renderer.SupportsDLSS();
    support.dlssRR = g_renderer.SupportsDLSSRR();
    support.xess = g_renderer.SupportsXeSS();
    return support;
}

mathstl::Vector2 CalculateRenderResolution(const mathstl::Vector2& swapchainResolution, const RendererState& renderState)
{
    const f32 scale = static_cast<f32>(AA::ResolveRenderScalePercent(renderState, GetAASupport())) / 100.0f;
    return {static_cast<f32>(stltype::max(1u, static_cast<u32>(swapchainResolution.x * scale))),
            static_cast<f32>(stltype::max(1u, static_cast<u32>(swapchainResolution.y * scale)))};
}

void RecordTemporalResourceInitialLayouts(CommandBuffer* pCmdBuffer, RGResourceRegistry& registry)
{
    auto transitionInitial = [pCmdBuffer](Texture* pTex, ImageLayout newLayout)
    {
        if (!pTex)
            return;
        ImageLayoutTransitionCmd cmd(pTex);
        cmd.oldLayout = ImageLayout::UNDEFINED;
        cmd.newLayout = newLayout;
        TextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::UNDEFINED, newLayout);
        pCmdBuffer->RecordCommand(cmd);
    };

    transitionInitial(registry.GetShadowMap().pTexture, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);

    static const RGResourceID ids[] = {RGResourceID::MainDepth,
                                       RGResourceID::GBufferVelocity,
                                       RGResourceID::TemporalResolve,
                                       RGResourceID::TAAHistory,
                                       RGResourceID::GBufferPostAAColor,
                                       RGResourceID::ScreenSpaceShadows,
                                       RGResourceID::BloomMip0,
                                       RGResourceID::BloomMip1,
                                       RGResourceID::BloomMip2,
                                       RGResourceID::BloomMip3,
                                       RGResourceID::BloomMip4,
                                       RGResourceID::RTReflections,
                                       RGResourceID::RTAOOutput,
                                       RGResourceID::RTAccumulation,
                                       RGResourceID::GBufferDebug,
                                       // DLSSPass reads it even when DLSSExposurePass is off
                                       RGResourceID::DLSSExposure};
    for (const RGResourceID id : ids)
    {
        const RGResourceHandle h = registry.FindByID(id);
        if (h == kInvalidRGHandle)
            continue;
        const ImageLayout layout = id == RGResourceID::MainDepth ? ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL
                                                                 : ImageLayout::SHADER_READ_ONLY_OPTIMAL;
        Texture* pTex = registry.Resolve(h);
        Texture* pHist = registry.ResolveHistory(h);
        transitionInitial(pTex, layout);
        // Ping-pong history is sampled before anything writes it
        if (pHist != pTex)
            transitionInitial(pHist, layout);
        registry.SetResourceLayout(h, layout);
        registry.SetHistoryResourceLayout(h, layout);
    }
}
} // namespace

// Helper implementations to break up large functions
void PassManager::InitResourceManagerAndCallbacks()
{
    m_resourceManager.Init();
    m_rtSceneManager.Init(&m_resourceManager, g_renderer.GetQueueFamilyIndices().graphicsFamily.value());
    ResetSceneGeometry();

    g_engine.GetEventSystem().AddShaderHotReloadEventCallback([this](const auto&) { RebuildPipelinesForAllPasses(); });

    // Create pass objects
    AddPass(stltype::make_unique<RenderPasses::LightTransformComputePass>());
    AddPass(stltype::make_unique<RenderPasses::TileAssignmentComputePass>());
    AddPass(stltype::make_unique<RenderPasses::ClusterGeneratorComputePass>());
    AddPass(stltype::make_unique<RenderPasses::LightGridComputePass>());
    AddPass(stltype::make_unique<RenderPasses::CSMPass>());
    AddPass(stltype::make_unique<RenderPasses::DepthPrePass>());
    AddPass(stltype::make_unique<RenderPasses::StaticMainMeshPass>());
    auto pDebugShapePass = stltype::make_unique<RenderPasses::DebugShapePass>();
    m_pDebugShapePass = pDebugShapePass.get();
    AddPass(stltype::move(pDebugShapePass));
    AddPass(stltype::make_unique<RenderPasses::ScreenSpaceShadowPass>());
    AddPass(stltype::make_unique<RenderPasses::ClusterDebugPass>());
    AddPass(stltype::make_unique<RenderPasses::LightingPass>());
    // RT passes build ray-query pipelines, which devices without ray tracing (MoltenVK) can't create
    if (g_renderer.SupportsRayTracing())
    {
        AddPass(stltype::make_unique<RenderPasses::RTReflectionsPass>());
        AddPass(stltype::make_unique<RenderPasses::RTAOPass>());
        AddPass(stltype::make_unique<RenderPasses::RTCompositePass>());
        AddPass(stltype::make_unique<RenderPasses::RTDebugViewPass>());
    }
    AddPass(stltype::make_unique<RenderPasses::TAAPass>());
    g_renderer.AddVendorUpscalerPasses(*this);
    AddPass(stltype::make_unique<RenderPasses::BloomPass>());
    AddPass(stltype::make_unique<RenderPasses::CompositPass>());
    AddPass(stltype::make_unique<RenderPasses::SMAAPass>());
    AddPass(stltype::make_unique<RenderPasses::ImGuiPass>());
}

void PassManager::CreateUBOsAndMap()
{
    m_frameResourceManager.Init();
    m_frameResourceManager.CreatePassObjectsAndLayouts();
    m_frameResourceManager.CreateFrameRendererContexts(m_imageAvailableSemaphores, m_imageAvailableFences);
}

void PassManager::InitPassesAndImGui()
{
    RecreateResizeDependentResources(g_renderer.GetSwapchainExtent());
}

bool PassManager::NeedsResizeDependentResourceRecreate(const mathstl::Vector2& swapchainResolution) const
{
    // kind of a sanity check so we dont need to track events with bools but also not perfect
    const auto& appRenderState = g_engine.GetApplicationState().GetCurrentApplicationState().renderState;
    const auto desiredRenderResolution = CalculateRenderResolution(swapchainResolution, appRenderState);
    return swapchainResolution.x != m_renderState.swapchainResolution.x ||
           swapchainResolution.y != m_renderState.swapchainResolution.y ||
           desiredRenderResolution.x != m_renderState.renderResolution.x ||
           desiredRenderResolution.y != m_renderState.renderResolution.y;
}

void PassManager::RecreateResizeDependentResources(const mathstl::Vector2& swapchainResolution)
{
    ScopedZone("PassManager::RecreateResizeDependentResources");

    const auto& renderState = g_engine.GetApplicationState().GetCurrentApplicationState().renderState;
    m_renderState.swapchainResolution = swapchainResolution;
    m_renderState.renderResolution = CalculateRenderResolution(swapchainResolution, renderState);
    m_renderState.recreatedThisFrame = true;

    // Recreate Shadow Maps
    {
        const auto csmCascades = renderState.directionalLightCascades;
        const auto csmResolution = renderState.csmResolution;
        RecreateShadowMaps(csmCascades, csmResolution);
        auto& shadowMapState = m_frameResourceManager.GetShadowMapState();
        shadowMapState.cascadeCount = csmCascades;
        shadowMapState.shadowMapExtents = csmResolution;
    }

    m_imguiRegistry.ReleaseGBufferIdsForNextFrame();
    m_renderGraph.GetRegistry().OnResize(m_renderState.renderResolution, m_renderState.swapchainResolution);
    m_renderGraph.GetRegistry().DeclareEngineResources();
    m_renderGraph.GetRegistry().AllocatePending();

    // Perform one-time layout transition and initial value setup for the newly recreated textures
    {
        CommandBuffer* pInitCmdBuffer = m_graphicsFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
        pInitCmdBuffer->SetName("One-time Resize Layout Setup Command Buffer");

        RecordTemporalResourceInitialLayouts(pInitCmdBuffer, m_renderGraph.GetRegistry());

        pInitCmdBuffer->Bake();
        g_renderer.GetQueueHandler().SubmitCommandBufferThisFrame({pInitCmdBuffer, QueueType::Graphics, 0});
        g_renderer.GetQueueHandler().DispatchAllRequests();
    }

    // Update Main Pass Data with new handles
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
            DescriptorSet::Cast(g_renderer.GetTextureManager().GetBindlessDescriptorSet());
        mainPassData.bufferDescriptors[UBO::DescriptorContentsType::BindlessImageArray] =
            DescriptorSet::Cast(g_renderer.GetTextureManager().GetBindlessImageDescriptorSet());
        mainPassData.bufferDescriptors[UBO::DescriptorContentsType::ClusterGrid] =
            m_frameResourceManager.GetFrameRendererContext(idx).clusterGridDescriptor;

        mainPassData.renderState = m_renderState;
        ++idx;
    }

    for (auto& pPass : m_passes)
    {
        if (m_passesInitialized)
        {
            pPass->RecreateResolutionDependentResources(m_resourceManager);
        }
        else
        {
            pPass->Init(m_resourceManager);
        }
    }
    m_passesInitialized = true;

    // Update UI Descriptors
    m_imguiRegistry.RegisterShadowMapTextures(m_renderGraph.GetRegistry().GetShadowMap());
    m_imguiRegistry.RegisterGBufferTextures(m_renderGraph.GetRegistry());
    m_imguiRegistry.RegisterRTTextures(m_renderGraph.GetRegistry());
}

void PassManager::PrepareMainPassDataForFrame(MainPassData& mainPassData, FrameRendererContext& ctx, u32 frameIdx)
{
    ScopedZone("PassManager::PrepareMainPassDataForFrame");
    mainPassData.pResourceManager = &m_resourceManager;
    mainPassData.mainView.descriptorSet = ctx.sharedDataUBODescriptor;
    mainPassData.mainView.viewport.x = 0.0f;
    mainPassData.mainView.viewport.y = 0.0f;
    mainPassData.mainView.viewport.width = m_renderState.renderResolution.x;
    mainPassData.mainView.viewport.height = m_renderState.renderResolution.y;
    mainPassData.mainView.viewport.minDepth = 0.0f;
    mainPassData.mainView.viewport.maxDepth = 1.0f;
    mainPassData.renderState = m_renderState;
    mainPassData.directionalLightShadowMap = m_renderGraph.GetRegistry().GetShadowMap();
    mainPassData.cascades = m_frameResourceManager.GetShadowMapState().cascadeCount;

    mainPassData.bufferDescriptors[UBO::DescriptorContentsType::GlobalInstanceData] =
        m_resourceManager.GetInstanceSSBODescriptorSet(frameIdx);
    mainPassData.bufferDescriptors[UBO::DescriptorContentsType::LightData] = ctx.tileArraySSBODescriptor;
    mainPassData.bufferDescriptors[UBO::DescriptorContentsType::BindlessTextureArray] =
        g_renderer.GetTextureManager().GetBindlessDescriptorSet();
    mainPassData.bufferDescriptors[UBO::DescriptorContentsType::ClusterGrid] = ctx.clusterGridDescriptor;
    mainPassData.pRTSceneManager = &m_rtSceneManager;

    m_imguiRegistry.RegisterMaterialTextures();
    m_imguiRegistry.PublishTextureViewerState(m_renderGraph.GetRegistry());
}

void PassManager::InitFrameContexts()
{
    const auto& indices = g_renderer.GetQueueFamilyIndices();
    m_graphicsFrameCtx.cmdPool = CommandPool::Create(indices.graphicsFamily.value());
    m_graphicsFrameCtx.cmdPool.SetName("Graphics Command Pool");
    m_computeFrameCtx.cmdPool = CommandPool::Create(indices.computeFamily.value());
    m_computeFrameCtx.cmdPool.SetName("Compute Command Pool");
}

void PassManager::SetupRenderGraph(const MainPassData& mainPassData, FrameRendererContext& ctx)
{
    ScopedZone("PassManager::SetupRenderGraph");

    m_renderGraph.BeginFrame(
        ctx.currentFrame, mainPassData.renderState.renderResolution, mainPassData.renderState.swapchainResolution);

    // The graph's first batch moves the swapchain image to color attachment before any pass runs
    if (ctx.pCurrentSwapchainTexture)
        m_renderGraph.GetRegistry().ImportTexture(
            RGResourceID::Swapchain, ctx.pCurrentSwapchainTexture, 0, ImageLayout::COLOR_ATTACHMENT_OPTIMAL);

    if (Texture* pShadowMap = m_renderGraph.GetRegistry().GetShadowMap().pTexture)
    {
        // Recorded transitions keep the texture's layout current; a freshly recreated map is still undefined
        m_renderGraph.GetRegistry().ImportTexture(RGResourceID::CSMShadowMap,
                                                  pShadowMap,
                                                  m_renderGraph.GetRegistry().GetShadowMap().bindlessHandle,
                                                  pShadowMap->GetInfo().layout);
    }

    for (auto& pPass : m_passes)
    {
        if (pPass && pPass->WantsToRender())
        {
            QueueType qType = pPass->GetQueueType();
            PassStage stage = pPass->GetPassStage();
            auto builder = m_renderGraph.AddNode(pPass->GetName(), qType, stage);
            pPass->Setup(builder, mainPassData);
            ConvolutionRenderPass* pRawPass = pPass.get();
            builder.SetExecuteCallback(
                [pRawPass](const MainPassData& d, const FrameRendererContext& c, const RGExecutionContext& execCtx)
                { pRawPass->RenderWithGraph(d, c, execCtx); });
        }
    }
}

void PassManager::CompileAndExecuteRenderGraph(const MainPassData& mainPassData,
                                              FrameRendererContext& ctx,
                                              Semaphore& imageAvailableSemaphore)
{
    ScopedZone("PassManager::CompileAndExecuteRenderGraph");

    m_renderGraph.Compile();
    UpdateGBufferUBO(ctx.currentFrame);
    m_renderGraph.BuildExecutionBatches(ctx);

    u32 graphicsBatchCount = 0;
    u32 computeBatchCount = 0;
    for (const auto& b : m_renderGraph.GetExecutionBatches())
    {
        if (b.queueType == QueueType::Graphics)
            graphicsBatchCount++;
        else if (b.queueType == QueueType::Compute)
            computeBatchCount++;
    }

    stltype::vector<CommandBuffer*> graphicsCmds;
    graphicsCmds.reserve(graphicsBatchCount);
    for (u32 i = 0; i < graphicsBatchCount; ++i)
    {
        graphicsCmds.push_back(GetBatchCommandBuffer(QueueType::Graphics, ctx.currentFrame, i));
    }

    stltype::vector<CommandBuffer*> computeCmds;
    computeCmds.reserve(computeBatchCount);
    for (u32 i = 0; i < computeBatchCount; ++i)
    {
        computeCmds.push_back(GetBatchCommandBuffer(QueueType::Compute, ctx.currentFrame, i));
    }

    m_gpuTimingQuery.ClearRunFlags(ctx.currentFrame);
    m_renderGraph.Execute(mainPassData, ctx, &imageAvailableSemaphore, graphicsCmds, computeCmds, &m_gpuTimingQuery);
    g_renderer.GetQueueHandler().FlushGraphicsComputeBuffers();
}

CommandBuffer* PassManager::GetBatchCommandBuffer(QueueType queueType, u32 frameIdx, u32 batchIdx)
{
    const bool isGraphics = queueType == QueueType::Graphics;
    QueueFrameContext& frameCtx = isGraphics ? m_graphicsFrameCtx : m_computeFrameCtx;
    auto& list = frameCtx.batchCmdBuffers[frameIdx];
    while (list.size() <= batchIdx)
    {
        CommandBuffer* pCmd = frameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
        pCmd->SetName((isGraphics ? "Graphics Batch CB " : "Async Compute Batch CB ") +
                      stltype::to_string(list.size()));
        pCmd->SetQueueType(queueType);
        list.push_back(pCmd);
    }
    CommandBuffer* pBuf = list[batchIdx];
    pBuf->ResetBuffer();
    pBuf->SetFrameIdx(frameIdx);
    return pBuf;
}

void PassManager::UpdateAAFrameConfig(u32 frameIdx)
{
    const auto& renderState = g_engine.GetApplicationState().GetCurrentApplicationState().renderState;
    const bool rtReflectionsActive = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTEnabled) &&
                                     mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTReflectionsEnabled) &&
                                     m_rtSceneManager.HasReadyTLAS(frameIdx);

    AA::FrameConfig config = AA::Resolve(renderState, GetAASupport(), rtReflectionsActive);
    // Any technique switch (incl. RR on/off), resize, scene change or UI request invalidates history
    config.temporalReset = m_renderState.recreatedThisFrame || config.temporal != m_lastTemporal ||
                           renderState.temporalResetGeneration != m_lastTemporalResetGeneration;
    m_lastTemporal = config.temporal;
    m_lastTemporalResetGeneration = renderState.temporalResetGeneration;
    AA::SetCurrent(config);
}

void PassManager::UpdateGBufferUBO(u32 frameIdx)
{
    const auto& reg = m_renderGraph.GetRegistry();
    const auto& appRenderState = g_engine.GetApplicationState().GetCurrentApplicationState().renderState;

    UBO::GBufferPostProcessUBO gbufferUBO{};
    gbufferUBO.gbufferAlbedoIdx = reg.ResolveBindlessByID(RGResourceID::GBufferAlbedo);
    gbufferUBO.gbufferNormalIdx = reg.ResolveBindlessByID(RGResourceID::GBufferNormal);
    gbufferUBO.gbufferMaterialIdx = reg.ResolveBindlessByID(RGResourceID::GBufferMaterial);
    gbufferUBO.gbufferRoughnessIdx = reg.ResolveBindlessByID(RGResourceID::GBufferRoughness);
    gbufferUBO.gbufferVelocityIdx = reg.ResolveBindlessByID(RGResourceID::GBufferVelocity);
    gbufferUBO.depthBufferIdx = reg.ResolveBindlessByID(RGResourceID::MainDepth);
    gbufferUBO.lastFrameDepthIdx = reg.ResolveHistoryBindlessByID(RGResourceID::MainDepth);
    gbufferUBO.sceneColorIdx = reg.ResolveBindlessByID(RGResourceID::GBufferThisFrameColor);
    gbufferUBO.taaHistoryIdx = reg.ResolveHistoryBindlessByID(RGResourceID::TAAHistory);
    gbufferUBO.taaOutputIdx = reg.ResolveBindlessByID(RGResourceID::TAAHistory);
    gbufferUBO.compositeInputIdx = reg.ResolveBindlessByID(AA::CompositeInput(appRenderState, AA::Current()));
    gbufferUBO.rtDebugViewIdx = reg.ResolveBindlessByID(RGResourceID::GBufferDebug);
    gbufferUBO.bloomResultIdx = appRenderState.bloom.enabled ? reg.ResolveBindlessByID(RGResourceID::BloomMip0) : 0;
    // Same check SetupRenderGraph used to add the pass this frame
    gbufferUBO.debugOverlayIdx =
        m_pDebugShapePass->WantsToRender() ? reg.ResolveBindlessByID(RGResourceID::DebugOverlay) : 0;

    m_frameResourceManager.GetGBufferPostProcessUBO().Write(frameIdx, gbufferUBO);
}

void PassManager::Init()
{
    InitResourceManagerAndCallbacks();
    CreateUBOsAndMap();
    InitFrameContexts();
    InitPassesAndImGui();
    m_gpuTimingQuery.Init(128);
    for (auto& pPass : m_passes)
    {
        pPass->SetTimingQuery(&m_gpuTimingQuery);
    }

    // One Tracy context per queue; the setup buffers are reset again before their first frame
    g_renderer.GetTracyGPUManager().Init(GetBatchCommandBuffer(QueueType::Graphics, 0, 0));
    g_renderer.GetTracyGPUManager().Init(GetBatchCommandBuffer(QueueType::Compute, 0, 0));
}

void PassManager::ExecutePasses(u32 frameIdx)
{
    g_renderer.BeginFrame(frameIdx);

    auto& ctx = m_frameResourceManager.GetFrameRendererContext(m_currentSwapChainIdx);
    auto& mainPassData = m_mainPassData.at(m_currentSwapChainIdx);
    auto& imageAvailableSemaphore = m_imageAvailableSemaphores.at(frameIdx);

    ctx.currentFrame = frameIdx;
    ctx.pCurrentSwapchainTexture = Texture::Cast(&g_renderer.GetTextureManager().GetSwapChainTextures().at(m_currentSwapChainIdx));

    g_renderer.GetQueueHandler().SubmitUploads(frameIdx);
    RebuildMeshDataForSlot(frameIdx, ctx);
    // TLAS slots follow the frame slot, the same index the RT passes read them with
    m_rtSceneManager.Update(frameIdx, m_frameResourceManager);
    PrepareMainPassDataForFrame(mainPassData, ctx, frameIdx);
    SetupRenderGraph(mainPassData, ctx);
    CompileAndExecuteRenderGraph(mainPassData, ctx, imageAvailableSemaphore);

    AsyncQueueHandler::PresentRequest presentRequest{.pWaitSemaphore = ctx.renderingFinishedSemaphore,
                                                     .swapChainImageIdx = m_currentSwapChainIdx};

    g_renderer.GetQueueHandler().SubmitSwapchainPresentRequestForThisFrame(presentRequest);
    m_renderState.recreatedThisFrame = false;
}

void PassManager::ReadAndPublishTimingResults(u32 frameIdx)
{
    if (!m_gpuTimingQuery.IsEnabled())
        return;
    m_gpuTimingQuery.ReadResults(frameIdx);

    const auto& results = m_gpuTimingQuery.GetResults();
    f32 totalTime = m_gpuTimingQuery.GetTotalGPUTimeMs();

    stltype::vector<PassTimingStat> passTimings;
    passTimings.reserve(results.size());
    for (const auto& r : results)
        passTimings.push_back({r.passName, r.gpuTimeMs, r.startMs, r.endMs, r.queueFamilyIndex, r.wasRun});

    const u64 usedVram = g_renderer.GetGPUMemoryManager().GetUsedVram();

    g_engine.GetApplicationState().RegisterUpdateFunction(
        [passTimings = stltype::move(passTimings), totalTime, usedVram](ApplicationState& state)
        {
            state.renderState.passTimings = stltype::move(passTimings);
            state.renderState.totalGPUTimeMs = totalTime;
            state.renderState.usedVramBytes = usedVram;
        });
}

PassManager::~PassManager()
{
    g_renderer.GetTracyGPUManager().Destroy();

    m_rtSceneManager.Reset();

    m_gpuTimingQuery.Destroy();
}

void PassManager::AddPass(stltype::unique_ptr<ConvolutionRenderPass>&& pass)
{
    m_passes.push_back(stltype::move(pass));
}

void PassManager::SetEntityMeshDataForFrame(EntityMeshDataMap&& data, u32 frameIdx)
{
    m_frameResourceManager.SetEntityMeshDataForFrame(stltype::move(data), frameIdx);
}
void PassManager::SetEntityTransformDataForFrame(TransformSystemData&& data, u32 frameIdx)
{
    m_frameResourceManager.SetEntityTransformDataForFrame(stltype::move(data), frameIdx);
}
void PassManager::SetLightDataForFrame(PointLightVector&& data, DirLightVector&& dirLights, u32 frameIdx)
{
    m_frameResourceManager.SetLightDataForFrame(stltype::move(data), stltype::move(dirLights), frameIdx);
}
void PassManager::SetLightDeltaForFrame(stltype::vector<LightDeltaUpdate>&& updates,
                                        bool dirLightDirty,
                                        const DirectionalRenderLight& dirLight,
                                        u32 frameIdx)
{
    m_frameResourceManager.SetLightDeltaForFrame(stltype::move(updates), dirLightDirty, dirLight, frameIdx);
}
void PassManager::SetSharedData(RenderView&& mainView, u32 frameIdx)
{
    m_frameResourceManager.SetSharedData(stltype::move(mainView), frameIdx);
}
void PassManager::PreProcessDataForCurrentFrame(u32 frameIdx, u64 jitterFrameNumber)
{
    ScopedZone("PassManager::PreProcessDataForCurrentFrame");
    m_frameResourceManager.GetFrameRendererContext(m_currentSwapChainIdx).frameCounter = jitterFrameNumber;
    // The light grid pass sums the cluster lists on the GPU; this slot's previous frame has finished
    {
        const auto& renderState = g_engine.GetApplicationState().GetCurrentApplicationState().renderState;
        u32 totalClusters = renderState.clusterCount.x * renderState.clusterCount.y * renderState.clusterCount.z;
        if (totalClusters > 0)
        {
            const u32 totalLightsInClusters = m_resourceManager.ReadClusterLightTotal(frameIdx);
            f32 avgLights = static_cast<f32>(totalLightsInClusters) / static_cast<f32>(totalClusters);
            g_engine.GetApplicationState().RegisterUpdateFunction([avgLights](ApplicationState& state)
                                                        { state.renderState.avgLightsPerCluster = avgLights; });
        }
    }

    m_renderGraph.GetRegistry().RotateHistory(frameIdx);
    m_imguiRegistry.PublishTextureViewerState(m_renderGraph.GetRegistry());
    UpdateAAFrameConfig(frameIdx);

    m_frameResourceManager.PreProcessDataForCurrentFrame(frameIdx, jitterFrameNumber, m_currentSwapChainIdx, this);
}

void PassManager::ResetSceneGeometry()
{
    m_resourceManager.ClearGeometryCaches();
    m_rtSceneManager.Reset();
}

void PassManager::ResetSceneState()
{
    m_frameResourceManager.ClearGeometryCaches();
    m_imguiRegistry.ReleaseMaterialTextures();
    ResetSceneGeometry();
    // Passes keep drawing the old scene's indirect commands until they are rebuilt empty
    PreProcessMeshData({});

    g_engine.GetApplicationState().RegisterUpdateFunction(
        [](ApplicationState& state)
        {
            ++state.renderState.temporalResetGeneration;
        });
}

bool PassManager::BlockUntilPassesFinished(u32 frameIdx)
{
    ScopedZone("Waiting for passes to finish (block until finished)");
    // Waits for the last frame that used this slot (N-2); CPU-written UBOs keep one copy per slot
    g_renderer.GetQueueHandler().WaitForFences(frameIdx);

    auto& fence = m_imageAvailableFences.at(frameIdx);
    auto& sem = m_imageAvailableSemaphores.at(frameIdx);
    fence.Reset();
    const auto acquireStatus =
        SRF::QueryImageForPresentationFromMainSwapchain<RenderAPI>(sem, fence, m_currentSwapChainIdx);
    if (acquireStatus == SRF::SwapchainAcquireStatus::NeedsRecreate)
    {
        g_engine.GetEventSystem().OnSwapchainRecreation({});
        return false;
    }
    if (acquireStatus != SRF::SwapchainAcquireStatus::Acquired)
    {
        DEBUG_LOG_ERR("Swapchain image acquisition failed.");
        return false;
    }
    fence.WaitFor();
    fence.Reset();
    return true;
}

void PassManager::RebuildPipelinesForAllPasses()
{
    if (!g_renderer.GetShaderManager().ReloadAllShaders())
        return;
    for (auto& pass : m_passes)
    {
        pass->BuildPipelines();
    }
}

void PassManager::PreProcessMeshData(const stltype::vector<PassMeshData>& meshes)
{
    // Each slot rebuilds right before it records again; the other slot may still be on the GPU
    m_pendingMeshData = meshes;
    m_meshRebuildSlotMask = (1u << FRAMES_IN_FLIGHT) - 1u;
}

void PassManager::RebuildMeshDataForSlot(u32 frameIdx, FrameRendererContext& ctx)
{
    const u32 slotBit = 1u << frameIdx;
    if ((m_meshRebuildSlotMask & slotBit) == 0)
        return;

    ScopedZone("PassManager::RebuildMeshDataForSlot");
    ctx.pResourceManager = &m_resourceManager;
    for (auto& pass : m_passes)
    {
        pass->RebuildInternalData(m_pendingMeshData, ctx, frameIdx);
        pass->NameResources(pass->GetName());
    }
    m_meshRebuildSlotMask &= ~slotBit;
    if (m_meshRebuildSlotMask == 0)
        m_pendingMeshData.clear();
}

void PassManager::RecreateShadowMaps(u32 cascades, const mathstl::Vector2& extents)
{
    m_renderState.recreatedThisFrame = true;
    m_imguiRegistry.ReleaseShadowMapIdsForNextFrame();
    m_renderGraph.GetRegistry().RecreateShadowMap(cascades, extents);

    for (auto& pass : m_passes)
    {
        if (auto* cp = dynamic_cast<CSMPass*>(pass.get()))
            cp->SetCascadeCount(cascades);
    }
    if (m_passesInitialized)
    {
        m_imguiRegistry.RegisterShadowMapTextures(m_renderGraph.GetRegistry().GetShadowMap());
    }
}
