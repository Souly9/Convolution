#include "PassManager.h"
#include "AA/DLSSPass.h"
#include "AA/DLSSRRPass.h"
#include "AA/SMAAPass.h"
#include "AA/TAAPass.h"
#include "AA/XeSSPass.h"
#include "ClusteredShading/ClusterDebugPass.h"
#include "ClusteredShading/ClusterGeneratorComputePass.h"
#include "ClusteredShading/LightGridComputePass.h"
#include "ClusteredShading/LightTransformComputePass.h"
#include "ClusteredShading/TileAssignmentComputePass.h"
#include "Compositing/CompositPass.h"
#include "Compositing/LightingPass.h"
#include "Core/Global/FrameGlobals.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Global/State/States.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/GPUTimingQuery.h"
#include "Core/Rendering/Core/Nvidia/StreamlineManager.h"
#include "Core/Rendering/Core/ProfilingUtils.h"
#include "Core/Rendering/Core/RenderGraph/RenderGraph.h"
#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/TracyManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Vulkan/Utils/VkEnumHelpers.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "Core/Rendering/Vulkan/VkTracyManager.h"
#include "Core/Rendering/Vulkan/XeSS/XeSSManager.h"
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
mathstl::Vector2 CalculateRenderResolution(const mathstl::Vector2& swapchainResolution, u32 upscalingPercentage)
{
    const f32 scale = static_cast<f32>(upscalingPercentage) / 100.0f;
    return {static_cast<f32>(stltype::max(1u, static_cast<u32>(swapchainResolution.x * scale))),
            static_cast<f32>(stltype::max(1u, static_cast<u32>(swapchainResolution.y * scale)))};
}
} // namespace

// Helper implementations to break up large functions
void PassManager::InitResourceManagerAndCallbacks()
{
    m_resourceManager.Init();
    m_rtSceneManager.Init(&m_resourceManager, VkGlobals::GetQueueFamilyIndices().graphicsFamily.value());
    auto registerSceneGeometry = [this]()
    {
        m_rtSceneManager.Reset();
        m_resourceManager.UploadSceneGeometry(g_pMeshManager->GetMeshes());
        m_rtSceneManager.RegisterSceneMeshes(g_pMeshManager->GetMeshes());
    };

    g_pEventSystem->AddSceneLoadedEventCallback([registerSceneGeometry](const SceneLoadedEventData&)
                                                { registerSceneGeometry(); });

    const Scene* pCurrentScene = g_pApplicationState->GetCurrentScene();
    if (pCurrentScene != nullptr && pCurrentScene->IsFullyLoaded())
    {
        registerSceneGeometry();
    }

    g_pEventSystem->AddShaderHotReloadEventCallback([this](const auto&) { RebuildPipelinesForAllPasses(); });

    // Create pass objects
    AddPass(stltype::make_unique<RenderPasses::LightTransformComputePass>());
    AddPass(stltype::make_unique<RenderPasses::TileAssignmentComputePass>());
    AddPass(stltype::make_unique<RenderPasses::ClusterGeneratorComputePass>());
    AddPass(stltype::make_unique<RenderPasses::LightGridComputePass>());
    AddPass(stltype::make_unique<RenderPasses::CSMPass>());
    AddPass(stltype::make_unique<RenderPasses::DepthPrePass>());
    AddPass(stltype::make_unique<RenderPasses::StaticMainMeshPass>());
    AddPass(stltype::make_unique<RenderPasses::DebugShapePass>());
    AddPass(stltype::make_unique<RenderPasses::ScreenSpaceShadowPass>());
    AddPass(stltype::make_unique<RenderPasses::ClusterDebugPass>());
    AddPass(stltype::make_unique<RenderPasses::LightingPass>());
    AddPass(stltype::make_unique<RenderPasses::RTReflectionsPass>());
    AddPass(stltype::make_unique<RenderPasses::RTAOPass>());
    AddPass(stltype::make_unique<RenderPasses::RTCompositePass>());
    AddPass(stltype::make_unique<RenderPasses::RTDebugViewPass>());
    AddPass(stltype::make_unique<RenderPasses::TAAPass>());
    if (Nvidia::StreamlineManager::IsDLSSSupported())
    {
        AddPass(stltype::make_unique<RenderPasses::DLSSPass>());
    }
    if (Nvidia::StreamlineManager::IsDLSSRRSupported())
    {
        AddPass(stltype::make_unique<RenderPasses::DLSSRRPass>());
    }
    if (VulkanXeSS::XeSSManager::IsSupported())
    {
        AddPass(stltype::make_unique<RenderPasses::XeSSPass>());
    }
    AddPass(stltype::make_unique<RenderPasses::BloomPass>());
    AddPass(stltype::make_unique<RenderPasses::CompositPass>());
    AddPass(stltype::make_unique<RenderPasses::SMAAPass>());
    AddPass(stltype::make_unique<RenderPasses::ImGuiPass>());
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
    RecreateResizeDependentResources(FrameGlobals::GetSwapChainExtent(), true);
}

bool PassManager::NeedsResizeDependentResourceRecreate(const mathstl::Vector2& swapchainResolution) const
{
    // kind of a sanity check so we dont need to track events with bools but also not perfect
    const auto& appRenderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    const auto desiredRenderResolution =
        CalculateRenderResolution(swapchainResolution, appRenderState.upscalingPercentage);
    return swapchainResolution.x != m_renderState.swapchainResolution.x ||
           swapchainResolution.y != m_renderState.swapchainResolution.y ||
           desiredRenderResolution.x != m_renderState.renderResolution.x ||
           desiredRenderResolution.y != m_renderState.renderResolution.y;
}

void PassManager::RecreateResizeDependentResources(const mathstl::Vector2& swapchainResolution, bool swapchainRecreated)
{
    ScopedZone("PassManager::RecreateResizeDependentResources");
    (void)swapchainRecreated;

    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    m_renderState.swapchainResolution = swapchainResolution;
    m_renderState.renderResolution = CalculateRenderResolution(swapchainResolution, renderState.upscalingPercentage);
    m_renderState.recreatedThisFrame = true;
    g_pApplicationState->RegisterUpdateFunction(
        [renderResolution = m_renderState.renderResolution,
         swapchainResolution = m_renderState.swapchainResolution](ApplicationState& state)
        {
            state.renderState.renderResolution = renderResolution;
            state.renderState.swapchainResolution = swapchainResolution;
            state.renderState.renderTargetsRecreatedThisFrame = true;
        });

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

        m_transitionRecorder.RecordTemporalResourceInitialLayouts(
            pInitCmdBuffer, m_renderGraph.GetRegistry(), m_frameResourceManager.GetDLSSExposureStagingBuffer());

        pInitCmdBuffer->Bake();
        g_pQueueHandler->SubmitCommandBufferThisFrame({pInitCmdBuffer, QueueType::Graphics, 0});
        g_pQueueHandler->DispatchAllRequests();
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
            DescriptorSet::Cast(g_pTexManager->GetBindlessDescriptorSet());
        mainPassData.bufferDescriptors[UBO::DescriptorContentsType::BindlessImageArray] =
            DescriptorSet::Cast(g_pTexManager->GetBindlessImageDescriptorSet());
        mainPassData.bufferDescriptors[UBO::DescriptorContentsType::ClusterGrid] =
            m_frameResourceManager.GetFrameRendererContext(idx).clusterGridDescriptor;

        mainPassData.renderState = m_renderState;
        ++idx;
    }

    UpdateFrameFeatureState(m_mainPassData[0]);
    UpdateGBufferUBO(m_mainPassData[0]);
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        m_frameResourceManager.GetFrameRendererContext(i).gbufferPostProcessDescriptor->WriteBufferUpdate(
            m_frameResourceManager.GetGBufferPostProcessUBO(), s_globalGbufferPostProcessUBOSlot);
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

bool PassManager::AnyPassWantsToRender() const
{
    for (const auto& pass : m_passes)
    {
        if (pass->WantsToRender())
            return true;
    }
    return false;
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
        g_pTexManager->GetBindlessDescriptorSet();
    mainPassData.bufferDescriptors[UBO::DescriptorContentsType::ClusterGrid] = ctx.clusterGridDescriptor;
    mainPassData.pRTSceneManager = &m_rtSceneManager;

    ctx.pDLSSExposureTexture = m_renderGraph.GetRegistry().ResolveByID(RGResourceID::DLSSExposure);

    m_imguiRegistry.RegisterMaterialTextures();
    m_imguiRegistry.PublishGBufferTextureState(m_renderGraph.GetRegistry());
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
        }
        m_computeFrameCtx.initialized = true;
    }
}

void PassManager::SetupRenderGraph(const MainPassData& mainPassData, FrameRendererContext& ctx)
{
    ScopedZone("PassManager::SetupRenderGraph");

    UpdateFrameFeatureState(mainPassData);

    CommandBuffer* pMainGraphicsWorkBuffer = m_graphicsFrameCtx.cmdBuffers[ctx.currentFrame];
    CommandBuffer* pComputeCmdBuffer = m_computeFrameCtx.cmdBuffers[ctx.currentFrame];

    pMainGraphicsWorkBuffer->ResetBuffer();
    pComputeCmdBuffer->ResetBuffer();

    pMainGraphicsWorkBuffer->SetFrameIdx(ctx.currentFrame);
    pComputeCmdBuffer->SetFrameIdx(ctx.currentFrame);

    m_renderGraph.BeginFrame(
        ctx.currentFrame, mainPassData.renderState.renderResolution, mainPassData.renderState.swapchainResolution);
    m_renderGraph.SetRTSceneAvailable(mainPassData.pRTSceneManager != nullptr &&
                                      mainPassData.pRTSceneManager->HasReadyTLAS(m_currentSwapChainIdx));

    if (ctx.pCurrentSwapchainTexture)
        m_renderGraph.GetRegistry().ImportTexture(
            RGResourceID::Swapchain, ctx.pCurrentSwapchainTexture, ImageLayout::UNDEFINED);

    if (m_renderGraph.GetRegistry().GetShadowMap().pTexture)
    {
        m_renderGraph.GetRegistry().ImportTexture(RGResourceID::CSMShadowMap,
                                                  m_renderGraph.GetRegistry().GetShadowMap().pTexture,
                                                  ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
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
    UpdateGBufferUBO(mainPassData);
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
        graphicsCmds.push_back(GetGraphicsCommandBuffer(ctx.currentFrame, i));
    }

    stltype::vector<CommandBuffer*> computeCmds;
    computeCmds.reserve(computeBatchCount);
    for (u32 i = 0; i < computeBatchCount; ++i)
    {
        computeCmds.push_back(GetComputeCommandBuffer(ctx.currentFrame, i));
    }

    m_gpuTimingQuery.ClearRunFlags(ctx.currentFrame);
    m_renderGraph.Execute(mainPassData, ctx, &imageAvailableSemaphore, graphicsCmds, computeCmds, &m_gpuTimingQuery);
    g_pQueueHandler->FlushGraphicsComputeBuffers();
}

void PassManager::RenderAllPassGroups(const MainPassData& mainPassData,
                                      FrameRendererContext& ctx,
                                      Semaphore& imageAvailableSemaphore)
{
    SetupRenderGraph(mainPassData, ctx);
    CompileAndExecuteRenderGraph(mainPassData, ctx, imageAvailableSemaphore);
}

CommandBuffer* PassManager::GetGraphicsCommandBuffer(u32 frameIdx, u32 batchIdx)
{
    auto& list = m_graphicsFrameCtx.batchCmdBuffers[frameIdx];
    while (list.size() <= batchIdx)
    {
        CommandBuffer* pCmd = m_graphicsFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
        pCmd->SetName("Graphics Batch CB " + stltype::to_string(list.size()));
        pCmd->SetQueueType(QueueType::Graphics);
        list.push_back(pCmd);
    }
    CommandBuffer* pBuf = list[batchIdx];
    pBuf->ResetBuffer();
    pBuf->SetFrameIdx(frameIdx);
    pBuf->SetQueueType(QueueType::Graphics);
    return pBuf;
}

CommandBuffer* PassManager::GetComputeCommandBuffer(u32 frameIdx, u32 batchIdx)
{
    auto& list = m_computeFrameCtx.batchCmdBuffers[frameIdx];
    while (list.size() <= batchIdx)
    {
        CommandBuffer* pCmd = m_computeFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
        pCmd->SetName("Async Compute Batch CB " + stltype::to_string(list.size()));
        pCmd->SetQueueType(QueueType::Compute);
        list.push_back(pCmd);
    }
    CommandBuffer* pBuf = list[batchIdx];
    pBuf->ResetBuffer();
    pBuf->SetFrameIdx(frameIdx);
    pBuf->SetQueueType(QueueType::Compute);
    return pBuf;
}

void PassManager::UpdateFrameFeatureState(const MainPassData& data)
{
    const auto& appRenderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    const bool rtReflectionsRequested =
        mathstl::isFlagSet(appRenderState.debugFlags, (u32)DebugFlags::RTEnabled) &&
        mathstl::isFlagSet(appRenderState.debugFlags, (u32)DebugFlags::RTReflectionsEnabled);

    const bool useRTReflections = rtReflectionsRequested && data.pRTSceneManager != nullptr &&
                                  data.pRTSceneManager->HasReadyTLAS(m_currentSwapChainIdx);

    const bool useRayReconstruction = Nvidia::StreamlineManager::IsDLSSRRSupported() &&
                                      appRenderState.rt.reflectionsUseRayReconstruction && useRTReflections;

    Nvidia::StreamlineManager::SetUseRayReconstructionThisFrame(useRayReconstruction);
}

void PassManager::UpdateGBufferUBO(const MainPassData& data)
{
    const auto& reg = m_renderGraph.GetRegistry();
    UBO::GBufferPostProcessUBO gbufferUBO{};
    gbufferUBO.gbufferAlbedoIdx = reg.ResolveBindlessByID(RGResourceID::GBufferAlbedo);
    gbufferUBO.gbufferNormalIdx = reg.ResolveBindlessByID(RGResourceID::GBufferNormal);
    gbufferUBO.gbufferTexCoordMatIdx = reg.ResolveBindlessByID(RGResourceID::GBufferUVMat);
    gbufferUBO.gbufferDebugIdx = reg.ResolveBindlessByID(RGResourceID::GBufferDebug);
    gbufferUBO.gbufferVelocityIdx = reg.ResolveBindlessByID(RGResourceID::GBufferVelocity);
    gbufferUBO.lastFrameVelocityIdx = reg.ResolveHistoryBindlessByID(RGResourceID::GBufferVelocity);
    gbufferUBO.depthBufferIdx = reg.ResolveBindlessByID(RGResourceID::MainDepth);
    gbufferUBO.lastFrameColorBufferIdx = reg.ResolveHistoryBindlessByID(RGResourceID::TemporalResolve);
    gbufferUBO.lastFrameDepthIdx = reg.ResolveHistoryBindlessByID(RGResourceID::MainDepth);
    gbufferUBO.gbufferResolveIdx = reg.ResolveBindlessByID(RGResourceID::TemporalResolve);
    gbufferUBO.rtDebugViewIdx = reg.ResolveBindlessByID(RGResourceID::GBufferDebug);
    gbufferUBO.rtReflectionsIdx = reg.ResolveBindlessByID(RGResourceID::RTReflections);
    gbufferUBO.rtaoIdx = reg.ResolveBindlessByID(RGResourceID::RTAOOutput);
    gbufferUBO.deferredLightingColorIdx = reg.ResolveBindlessByID(RGResourceID::GBufferThisFrameColor);
    const auto& appRenderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    gbufferUBO.bloomResultIdx = appRenderState.bloom.enabled ? reg.ResolveBindlessByID(RGResourceID::BloomMip0) : 0;
    const bool taaModeActive = appRenderState.aaType == AntialiasingType::TAA_SMAA;
    const bool smaaModeActive = appRenderState.aaType == AntialiasingType::SMAA;
    const bool taaDebugOrSeed = appRenderState.taaSeedHistoryFromCurrentColor ||
                                appRenderState.taaDebugMode == static_cast<u32>(TAADebugMode::CurrentColor) ||
                                appRenderState.taaDebugMode == static_cast<u32>(TAADebugMode::HistoryColor);

    const auto& rtState = appRenderState.rt;
    const bool rtReflectionsRequested =
        mathstl::isFlagSet(appRenderState.debugFlags, (u32)DebugFlags::RTEnabled) &&
        mathstl::isFlagSet(appRenderState.debugFlags, (u32)DebugFlags::RTReflectionsEnabled);

    const bool useRayReconstruction = Nvidia::StreamlineManager::GetUseRayReconstructionThisFrame();

    gbufferUBO.aaType = static_cast<uint32_t>(appRenderState.aaType);

    const auto rawColorHandle = reg.ResolveBindlessByID(RGResourceID::GBufferThisFrameColor);
    const auto postAAHandle = reg.ResolveBindlessByID(RGResourceID::GBufferPostAAColor);
    const auto resolveHandle = reg.ResolveBindlessByID(RGResourceID::TemporalResolve);

    gbufferUBO.thisFrameColorBufferIdx = rawColorHandle;

    if (appRenderState.aaType == AntialiasingType::SMAA && postAAHandle != 0)
    {
        gbufferUBO.finalTemporalColorBufferIdx = postAAHandle;
    }
    else if (appRenderState.aaType == AntialiasingType::TAA_SMAA)
    {
        gbufferUBO.finalTemporalColorBufferIdx = (postAAHandle != 0 && !taaDebugOrSeed) ? postAAHandle : resolveHandle;
    }
    else if (appRenderState.aaType == AntialiasingType::DLSS || appRenderState.aaType == AntialiasingType::XeSS)
    {
        gbufferUBO.finalTemporalColorBufferIdx = resolveHandle;
    }
    else
    {
        gbufferUBO.finalTemporalColorBufferIdx = rawColorHandle;
    }

    if (useRayReconstruction)
    {
        gbufferUBO.thisFrameColorBufferIdx = resolveHandle;
    }
    else if (rtReflectionsRequested && rtState.reflectionsDebugMode == RTReflectionDebugMode::ReflectionsOnly)
    {
        gbufferUBO.thisFrameColorBufferIdx = reg.ResolveBindlessByID(RGResourceID::RTReflections);
    }

    memcpy(m_frameResourceManager.GetMappedGBufferPostProcessUBO(), &gbufferUBO, sizeof(UBO::GBufferPostProcessUBO));
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

    if (VkGlobals::GetTracyManager() && !VkGlobals::GetTracyManager()->IsEnabled())
    {
        VkGlobals::GetTracyManager()->Init(VkGlobals::GetPhysicalDevice(),
                                           VkGlobals::GetLogicalDevice(),
                                           VkGlobals::GetGraphicsQueue(),
                                           m_graphicsFrameCtx.cmdBuffers[0]->GetRef());
    }
}

void PassManager::ExecutePasses(u32 frameIdx)
{
    Nvidia::StreamlineManager::AcquireNewFrameToken(frameIdx);

    auto& ctx = m_frameResourceManager.GetFrameRendererContext(m_currentSwapChainIdx);
    auto& mainPassData = m_mainPassData.at(m_currentSwapChainIdx);
    auto& imageAvailableSemaphore = m_imageAvailableSemaphores.at(frameIdx);

    ctx.imageIdx = m_currentSwapChainIdx;
    ctx.currentFrame = frameIdx;
    ctx.pCurrentSwapchainTexture = Texture::Cast(&g_pTexManager->GetSwapChainTextures().at(m_currentSwapChainIdx));

    g_pQueueHandler->DispatchAllRequests();
    m_resourceManager.FlushPendingMeshUploads(frameIdx, 256);
    m_rtSceneManager.Update(frameIdx, m_currentSwapChainIdx, m_frameResourceManager);
    g_pQueueHandler->DispatchAllRequests();
    PrepareMainPassDataForFrame(mainPassData, ctx, frameIdx);
    SetupRenderGraph(mainPassData, ctx);
    CompileAndExecuteRenderGraph(mainPassData, ctx, imageAvailableSemaphore);

    AsyncQueueHandler::PresentRequest presentRequest{.pWaitSemaphore = ctx.renderingFinishedSemaphore,
                                                     .swapChainImageIdx = m_currentSwapChainIdx};

    g_pQueueHandler->SubmitSwapchainPresentRequestForThisFrame(presentRequest);
    if (m_renderState.recreatedThisFrame)
    {
        m_renderState.recreatedThisFrame = false;
        g_pApplicationState->RegisterUpdateFunction([](ApplicationState& state)
                                                    { state.renderState.renderTargetsRecreatedThisFrame = false; });
    }
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

    u64 totalVram = 0, usedVram = 0;
    g_pGPUMemoryManager->GetVramStats(totalVram, usedVram);

    g_pApplicationState->RegisterUpdateFunction(
        [passTimings = stltype::move(passTimings), totalTime, totalVram, usedVram](ApplicationState& state)
        {
            state.renderState.passTimings = stltype::move(passTimings);
            state.renderState.totalGPUTimeMs = totalTime;
            state.renderState.totalVramBytes = totalVram;
            state.renderState.usedVramBytes = usedVram;
        });
}

PassManager::~PassManager()
{
    if (VkGlobals::GetTracyManager())
    {
        VkGlobals::GetTracyManager()->Destroy();
    }

    m_rtSceneManager.Reset();

    m_gpuTimingQuery.Destroy();
}

void PassManager::AddPass(stltype::unique_ptr<ConvolutionRenderPass>&& pass)
{
    m_passes.push_back(stltype::move(pass));
}

void PassManager::TransferPassData(const PassGeometryData& passData, u32 frameIdx)
{
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
    // Calculate average lights per cluster by reading final counts from the GPU buffer of the completed frame
    {
        const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
        u32 totalClusters = renderState.clusterCount.x * renderState.clusterCount.y * renderState.clusterCount.z;
        if (totalClusters > 0)
        {
            u32 totalLightsInClusters = 0;
            void* pMapped = m_frameResourceManager.GetLightClusterSSBO().MapMemory();
            if (pMapped)
            {
                char* pBase = static_cast<char*>(pMapped);
                u32* pOffsets = reinterpret_cast<u32*>(pBase + UBO::LightClusterOffsetsOffset);
                u32* pIndices = reinterpret_cast<u32*>(pBase + UBO::LightClusterIndicesOffset);

                for (u32 i = 0; i < totalClusters; ++i)
                {
                    u32 baseIdx = pOffsets[i];
                    if (baseIdx < MAX_LIGHT_INDICES)
                    {
                        totalLightsInClusters += pIndices[baseIdx];
                    }
                }
                m_frameResourceManager.GetLightClusterSSBO().UnmapMemory();
            }

            f32 avgLights = static_cast<f32>(totalLightsInClusters) / static_cast<f32>(totalClusters);
            g_pApplicationState->RegisterUpdateFunction([avgLights](ApplicationState& state)
                                                        { state.renderState.avgLightsPerCluster = avgLights; });
        }
    }

    m_renderGraph.GetRegistry().RotateHistory(frameIdx);
    m_imguiRegistry.PublishGBufferTextureState(m_renderGraph.GetRegistry());

    m_frameResourceManager.PreProcessDataForCurrentFrame(frameIdx, jitterFrameNumber, m_currentSwapChainIdx, this);
}

void PassManager::ResetSceneState()
{
    m_frameResourceManager.ClearGeometryCaches();
    m_resourceManager.ClearGeometryCaches();
    m_rtSceneManager.Reset();

    g_pApplicationState->RegisterUpdateFunction(
        [](ApplicationState& state)
        {
            state.renderState.taaSeedHistoryFromCurrentColor = true;
            state.renderState.renderTargetsRecreatedThisFrame = true;
        });
}

bool PassManager::BlockUntilPassesFinished(u32 frameIdx)
{
    ScopedZone("Waiting for passes to finish (block until finished)");
    // Wait for previous in-flight frame to finish as we dont double buffer UBOs etc
    g_pQueueHandler->WaitForFences(frameIdx);

    auto& fence = m_imageAvailableFences.at(frameIdx);
    auto& sem = m_imageAvailableSemaphores.at(frameIdx);
    fence.Reset();
    const auto acquireStatus =
        SRF::QueryImageForPresentationFromMainSwapchain<RenderAPI>(sem, fence, m_currentSwapChainIdx);
    if (acquireStatus == SRF::SwapchainAcquireStatus::NeedsRecreate)
    {
        if (g_pEventSystem != nullptr)
            g_pEventSystem->OnSwapchainRecreation({});
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
    if (!g_pShaderManager->ReloadAllShaders())
        return;
    for (auto& pass : m_passes)
    {
        pass->BuildPipelines();
    }
}

void PassManager::PreProcessMeshData(const stltype::vector<PassMeshData>& meshes, u32 lastFrame, u32 curFrame)
{
    auto& lastFrameCtx = m_frameResourceManager.GetFrameRendererContext(lastFrame);
    lastFrameCtx.pResourceManager = &m_resourceManager;
    for (auto& pass : m_passes)
    {
        for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
        {
            pass->RebuildInternalData(meshes, lastFrameCtx, i);
        }
        pass->NameResources(pass->GetName());
    }
}

void PassManager::RecreateShadowMaps(u32 cascades, const mathstl::Vector2& extents)
{
    m_renderState.recreatedThisFrame = true;
    g_pApplicationState->RegisterUpdateFunction([](ApplicationState& state)
                                                { state.renderState.renderTargetsRecreatedThisFrame = true; });
    m_imguiRegistry.ReleaseShadowMapIdsForNextFrame();
    m_renderGraph.GetRegistry().RecreateShadowMap(cascades, extents, m_frameResourceManager);

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
