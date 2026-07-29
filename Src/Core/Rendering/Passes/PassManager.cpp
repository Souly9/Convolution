#include "PassManager.h"
#include "Core/Rendering/Core/RenderGraph/RenderGraph.h"
#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"
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
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Vulkan/Utils/VkEnumHelpers.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "Core/Rendering/Vulkan/XeSS/XeSSManager.h"
#include "Core/SceneGraph/Scene.h"
#include "DebugShapePass.h"
#include "ImGuiPass.h"
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
    AddPass(PassType::RTReflectionsCompute, stltype::make_unique<RenderPasses::RTReflectionsPass>());
    AddPass(PassType::RTAOCompute, stltype::make_unique<RenderPasses::RTAOPass>());
    AddPass(PassType::RTComposite, stltype::make_unique<RenderPasses::RTCompositePass>());
    AddPass(PassType::PostProcess, stltype::make_unique<RenderPasses::RTDebugViewPass>());
    AddPass(PassType::TAA, stltype::make_unique<RenderPasses::TAAPass>());
    AddPass(PassType::SMAA, stltype::make_unique<RenderPasses::SMAAPass>());
    if (Nvidia::StreamlineManager::IsDLSSSupported())
    {
        AddPass(PassType::DLSS, stltype::make_unique<RenderPasses::DLSSPass>());
    }
    if (Nvidia::StreamlineManager::IsDLSSRRSupported())
    {
        AddPass(PassType::DLSS_RR, stltype::make_unique<RenderPasses::DLSSRRPass>());
    }
    if (VulkanXeSS::XeSSManager::IsSupported())
    {
        AddPass(PassType::XeSS, stltype::make_unique<RenderPasses::XeSSPass>());
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
    m_renderTargetManager.Recreate(m_renderState.renderResolution, m_renderState.swapchainResolution);
    m_renderTargetManager.GetAttachments().directionalLightShadowMap = m_shadowMapManager.GetShadowMap();
    m_rtResourceManager.Recreate(m_renderState.renderResolution);

    // Perform one-time layout transition and initial value setup for the newly recreated textures
    {
        CommandBuffer* pInitCmdBuffer = m_graphicsFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
        pInitCmdBuffer->SetName("One-time Resize Layout Setup Command Buffer");
        pInitCmdBuffer->ResetBuffer();

        m_transitionRecorder.RecordTemporalResourceInitialLayouts(pInitCmdBuffer,
                                                                  m_renderTargetManager.GetGBuffer(),
                                                                  m_renderTargetManager.GetDLSSExposureTexture(),
                                                                  m_renderTargetManager.GetDLSSExposureStagingBuffer());

        m_rtResourceManager.RecordOutputsToShaderRead(pInitCmdBuffer);

        pInitCmdBuffer->Bake();
        g_pQueueHandler->SubmitCommandBufferThisFrame({pInitCmdBuffer, QueueType::Graphics, 0});
        g_pQueueHandler->DispatchAllRequests();
        vkDeviceWaitIdle(VkGlobals::GetLogicalDevice());
        pInitCmdBuffer->Destroy();
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
        UpdateTemporalResources(mainPassData);
        mainPassData.depthBufferBindlessHandle = mainPassData.temporalResources.currentDepthHandle;
        mainPassData.pMainDepthTexture = mainPassData.temporalResources.pCurrentDepthTexture;
        mainPassData.pLastFrameDepthTexture = mainPassData.temporalResources.pHistoryDepthTexture;
        ++idx;
    }

    UpdateGBufferUBO(m_mainPassData[0]);
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        m_frameResourceManager.GetFrameRendererContext(i).gbufferPostProcessDescriptor->WriteBufferUpdate(
            m_frameResourceManager.GetGBufferPostProcessUBO(), s_globalGbufferPostProcessUBOSlot);
    }

    g_pTexManager->PostRender();

    // First creation owns full pass setup; later render-size changes only refresh cached resize state
    // TODO: FIX THIS AND REMOVE THE BOOL
    for (auto& [type, passes] : m_passes)
    {
        for (auto& pPass : passes)
        {
            if (m_passesInitialized)
            {
                pPass->RecreateResolutionDependentResources(m_renderTargetManager.GetAttachments(), m_resourceManager);
            }
            else
            {
                pPass->Init(m_renderTargetManager.GetAttachments(), m_resourceManager);
            }
        }
    }
    m_passesInitialized = true;

    // Update UI Descriptors
    m_imguiRegistry.RegisterShadowMapTextures(m_shadowMapManager.GetShadowMap());
    m_imguiRegistry.RegisterGBufferTextures(m_renderTargetManager.GetGBuffer(),
                                            m_renderTargetManager.GetScreenSpaceShadowTexture());
    m_imguiRegistry.RegisterRTTextures(m_rtResourceManager);
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
    mainPassData.pGbuffer = &m_renderTargetManager.GetGBuffer();
    mainPassData.mainView.descriptorSet = ctx.sharedDataUBODescriptor;
    mainPassData.mainView.viewport.x = 0.0f;
    mainPassData.mainView.viewport.y = 0.0f;
    mainPassData.mainView.viewport.width = m_renderState.renderResolution.x;
    mainPassData.mainView.viewport.height = m_renderState.renderResolution.y;
    mainPassData.mainView.viewport.minDepth = 0.0f;
    mainPassData.mainView.viewport.maxDepth = 1.0f;
    mainPassData.renderState = m_renderState;
    mainPassData.directionalLightShadowMap = m_shadowMapManager.GetShadowMap();
    mainPassData.cascades = m_frameResourceManager.GetShadowMapState().cascadeCount;
    mainPassData.depthBufferBindlessHandle = mainPassData.temporalResources.currentDepthHandle;
    mainPassData.pMainDepthTexture = mainPassData.temporalResources.pCurrentDepthTexture;
    mainPassData.pLastFrameDepthTexture = mainPassData.temporalResources.pHistoryDepthTexture;

    mainPassData.bufferDescriptors[UBO::DescriptorContentsType::GlobalInstanceData] =
        m_resourceManager.GetInstanceSSBODescriptorSet(frameIdx);
    mainPassData.bufferDescriptors[UBO::DescriptorContentsType::LightData] = ctx.tileArraySSBODescriptor;
    mainPassData.bufferDescriptors[UBO::DescriptorContentsType::BindlessTextureArray] =
        g_pTexManager->GetBindlessDescriptorSet();
    mainPassData.bufferDescriptors[UBO::DescriptorContentsType::ClusterGrid] = ctx.clusterGridDescriptor;
    mainPassData.pRTSceneManager = &m_rtSceneManager;
    const auto& rtDebugView = m_rtResourceManager.Get(RT::RTTextureType::DebugView);
    const auto& rtReflections = m_rtResourceManager.Get(RT::RTTextureType::Reflections);
    const auto& rtAO = m_rtResourceManager.Get(RT::RTTextureType::RTAO);
    const auto& rtAccum = m_rtResourceManager.Get(RT::RTTextureType::Accumulation);
    mainPassData.pRTDebugViewTexture = rtDebugView.pTexture;
    mainPassData.pRTReflectionsTexture = rtReflections.pTexture;
    mainPassData.pRTAOTexture = rtAO.pTexture;
    mainPassData.pRTAccumulationTexture = rtAccum.pTexture;
    mainPassData.rtDebugTextureHandle = rtDebugView.bindlessHandle;
    mainPassData.rtReflectionsTextureHandle = rtReflections.bindlessHandle;
    mainPassData.rtaoTextureHandle = rtAO.bindlessHandle;
    mainPassData.rtAccumulationTextureHandle = rtAccum.bindlessHandle;

    mainPassData.pScreenSpaceShadowTexture = m_renderTargetManager.GetScreenSpaceShadowTexture();
    mainPassData.screenSpaceShadows = m_renderTargetManager.GetScreenSpaceShadowBindlessHandle();
    mainPassData.pSMAAEdgesTexture = m_renderTargetManager.GetSMAAEdgesTexture();
    mainPassData.pSMAABlendTexture = m_renderTargetManager.GetSMAABlendTexture();
    mainPassData.smaaEdges = m_renderTargetManager.GetSMAAEdgesBindlessHandle();
    mainPassData.smaaBlend = m_renderTargetManager.GetSMAABlendBindlessHandle();

    ctx.pDLSSExposureTexture = m_renderTargetManager.GetDLSSExposureTexture();
}

void PassManager::UpdateTemporalResources(MainPassData& mainPassData)
{
    auto& temporal = mainPassData.temporalResources;
    auto& gbuffer = m_renderTargetManager.GetGBuffer();
    temporal.pCurrentColorTexture = gbuffer.Get(GBufferTextureType::GBufferThisFrameColor);
    temporal.pHistoryColorTexture = gbuffer.Get(GBufferTextureType::GBufferLastFrameColor);
    temporal.pResolveTexture = gbuffer.Get(GBufferTextureType::GBufferResolve);
    temporal.pPostAAColorTexture = gbuffer.Get(GBufferTextureType::GBufferPostAAColor);
    temporal.pCurrentDepthTexture = m_renderTargetManager.GetDepthTexture();
    temporal.pHistoryDepthTexture = m_renderTargetManager.GetLastFrameDepthTexture();
    temporal.currentColorHandle = gbuffer.GetHandle(GBufferTextureType::GBufferThisFrameColor);
    temporal.historyColorHandle = gbuffer.GetHandle(GBufferTextureType::GBufferLastFrameColor);
    temporal.resolveHandle = gbuffer.GetHandle(GBufferTextureType::GBufferResolve);
    temporal.postAAColorHandle = gbuffer.GetHandle(GBufferTextureType::GBufferPostAAColor);
    temporal.currentDepthHandle = m_renderTargetManager.GetDepthBindlessHandle();
    temporal.historyDepthHandle = m_renderTargetManager.GetLastFrameDepthBindlessHandle();
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
            m_graphicsFrameCtx.lightingCmdBuffers[i] =
                m_graphicsFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
            m_graphicsFrameCtx.lightingCmdBuffers[i]->SetName("Lighting Graphics Command Buffer " + numberString);
            m_graphicsFrameCtx.compositeCmdBuffers[i] =
                m_graphicsFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
            m_graphicsFrameCtx.compositeCmdBuffers[i]->SetName("Composite Graphics Command Buffer " + numberString);
            m_graphicsFrameCtx.depthPrePassCmdBuffers[i] =
                m_graphicsFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
            m_graphicsFrameCtx.depthPrePassCmdBuffers[i]->SetName("Depth Pre-Pass Graphics Command Buffer " +
                                                                  numberString);
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
            m_computeFrameCtx.rtComputeCmdBuffers[i] =
                m_computeFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
            m_computeFrameCtx.rtComputeCmdBuffers[i]->SetName("RT Async Compute Command Buffer " + numberString);
        }
        m_computeFrameCtx.initialized = true;
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

    CommandBuffer* pMainGraphicsWorkBuffer = m_graphicsFrameCtx.cmdBuffers[ctx.currentFrame];
    CommandBuffer* pComputeCmdBuffer = m_computeFrameCtx.cmdBuffers[ctx.currentFrame];
    CommandBuffer* pDepthWorkBuffer = m_graphicsFrameCtx.depthPrePassCmdBuffers[ctx.currentFrame];

    pMainGraphicsWorkBuffer->ResetBuffer();
    pComputeCmdBuffer->ResetBuffer();
    pDepthWorkBuffer->ResetBuffer();

    pMainGraphicsWorkBuffer->SetFrameIdx(ctx.currentFrame);
    pComputeCmdBuffer->SetFrameIdx(ctx.currentFrame);
    pDepthWorkBuffer->SetFrameIdx(ctx.currentFrame);

    m_renderGraph.BeginFrame(ctx.currentFrame, mainPassData.renderState.renderResolution, mainPassData.renderState.swapchainResolution);
    m_renderGraph.SetRTSceneAvailable(mainPassData.pRTSceneManager != nullptr && mainPassData.pRTSceneManager->HasReadyTLAS(m_currentSwapChainIdx));

    m_renderGraph.GetRegistry().ImportEngineResources(mainPassData, ctx, m_renderTargetManager);

    for (const auto& stage : PASS_SCHEDULE)
    {
        for (PassType groupType : stage.groups)
        {
            auto passIt = m_passes.find(groupType);
            if (passIt != m_passes.end())
            {
                for (auto& pPass : passIt->second)
                {
                    if (pPass && pPass->WantsToRender())
                    {
                        QueueType qType = pPass->GetQueueType();
                        auto builder = m_renderGraph.AddNode(pPass->GetName(), qType);
                        pPass->Setup(builder, mainPassData);
                        ConvolutionRenderPass* pRawPass = pPass.get();
                        builder.SetExecuteCallback([pRawPass](const MainPassData& d, const FrameRendererContext& c, const RGExecutionContext& execCtx) {
                            pRawPass->RenderWithGraph(d, c, execCtx);
                        });
                    }
                }
            }
        }
    }

    m_renderGraph.Compile();
    UpdateGBufferUBO(mainPassData);
    m_renderGraph.BuildExecutionBatches(ctx);

    u32 graphicsBatchCount = 0;
    u32 computeBatchCount = 0;
    for (const auto& b : m_renderGraph.GetExecutionBatches())
    {
        if (b.queueType == QueueType::Graphics) graphicsBatchCount++;
        else if (b.queueType == QueueType::Compute) computeBatchCount++;
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

    m_renderGraph.Execute(mainPassData, ctx, &imageAvailableSemaphore, graphicsCmds, computeCmds);
    g_pQueueHandler->FlushGraphicsComputeBuffers();
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
    const bool taaModeActive = appRenderState.aaType == AntialiasingType::TAA_SMAA;
    const bool smaaModeActive = appRenderState.aaType == AntialiasingType::SMAA;
    const bool taaDebugOrSeed = appRenderState.taaSeedHistoryFromCurrentColor ||
                                appRenderState.taaDebugMode == static_cast<u32>(TAADebugMode::CurrentColor) ||
                                appRenderState.taaDebugMode == static_cast<u32>(TAADebugMode::HistoryColor);

    const auto& rtState = appRenderState.rt;
    const bool rtReflectionsRequested =
        mathstl::isFlagSet(appRenderState.debugFlags, (u32)DebugFlags::RTEnabled) &&
        mathstl::isFlagSet(appRenderState.debugFlags, (u32)DebugFlags::RTReflectionsEnabled);
    const bool rtaoRequested = mathstl::isFlagSet(appRenderState.debugFlags, (u32)DebugFlags::RTEnabled) &&
                               mathstl::isFlagSet(appRenderState.debugFlags, (u32)DebugFlags::RTAOEnabled);

    const bool useRTReflections = rtReflectionsRequested && data.pRTSceneManager != nullptr &&
                                  data.pRTSceneManager->HasReadyTLAS(m_currentSwapChainIdx);
    const bool useRTAO =
        rtaoRequested && data.pRTSceneManager != nullptr && data.pRTSceneManager->HasReadyTLAS(m_currentSwapChainIdx);
    const bool useRayReconstruction = Nvidia::StreamlineManager::IsDLSSRRSupported() &&
                                      appRenderState.rt.reflectionsUseRayReconstruction && useRTReflections;

    gbufferUBO.thisFrameColorBufferIdx = reg.ResolveBindlessByID(RGResourceID::GBufferThisFrameColor);

    Nvidia::StreamlineManager::SetUseRayReconstructionThisFrame(useRayReconstruction);

    const auto postAAHandle = reg.ResolveBindlessByID(RGResourceID::GBufferPostAAColor);
    const auto resolveHandle = reg.ResolveBindlessByID(RGResourceID::TemporalResolve);

    gbufferUBO.finalTemporalColorBufferIdx = (((taaModeActive && !taaDebugOrSeed) || smaaModeActive) &&
                                              postAAHandle != 0 && !useRayReconstruction)
                                                 ? postAAHandle
                                                 : resolveHandle;

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
    for (auto& [type, passes] : m_passes)
    {
        for (auto& pPass : passes)
            pPass->SetTimingQuery(&m_gpuTimingQuery);
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
    RenderAllPassGroups(mainPassData, ctx, imageAvailableSemaphore);

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
        [passTimings = std::move(passTimings), totalTime, totalVram, usedVram](ApplicationState& state)
        {
            state.renderState.passTimings = std::move(passTimings);
            state.renderState.totalGPUTimeMs = totalTime;
            state.renderState.totalVramBytes = totalVram;
            state.renderState.usedVramBytes = usedVram;
        });
}

PassManager::~PassManager()
{
    m_rtSceneManager.Reset();
    m_rtResourceManager.Reset();
    m_gpuTimingQuery.Destroy();
}

void PassManager::AddPass(PassType type, stltype::unique_ptr<ConvolutionRenderPass>&& pass)
{
    m_passes[type].push_back(std::move(pass));
}

void PassManager::TransferPassData(const PassGeometryData& passData, u32 frameIdx)
{
}

void PassManager::SetEntityMeshDataForFrame(EntityMeshDataMap&& data, u32 frameIdx)
{
    m_frameResourceManager.SetEntityMeshDataForFrame(std::move(data), frameIdx);
}
void PassManager::SetEntityTransformDataForFrame(TransformSystemData&& data, u32 frameIdx)
{
    m_frameResourceManager.SetEntityTransformDataForFrame(std::move(data), frameIdx);
}
void PassManager::SetLightDataForFrame(PointLightVector&& data, DirLightVector&& dirLights, u32 frameIdx)
{
    m_frameResourceManager.SetLightDataForFrame(std::move(data), std::move(dirLights), frameIdx);
}
void PassManager::SetLightDeltaForFrame(stltype::vector<LightDeltaUpdate>&& updates,
                                        bool dirLightDirty,
                                        const DirectionalRenderLight& dirLight,
                                        u32 frameIdx)
{
    m_frameResourceManager.SetLightDeltaForFrame(std::move(updates), dirLightDirty, dirLight, frameIdx);
}
void PassManager::SetSharedData(RenderView&& mainView, u32 frameIdx)
{
    m_frameResourceManager.SetSharedData(std::move(mainView), frameIdx);
}
void PassManager::PreProcessDataForCurrentFrame(u32 frameIdx, u64 jitterFrameNumber)
{
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

    m_renderTargetManager.RotateHistory(frameIdx);
    for (auto& mainPassData : m_mainPassData)
    {
        UpdateTemporalResources(mainPassData);
    }
    m_imguiRegistry.PublishGBufferTextureState(m_renderTargetManager.GetGBuffer());

    m_frameResourceManager.PreProcessDataForCurrentFrame(frameIdx, jitterFrameNumber, m_currentSwapChainIdx, this);
}

void PassManager::ResetSceneState()
{
    m_frameResourceManager.ClearGeometryCaches();
    m_resourceManager.ClearGeometryCaches();
    m_rtSceneManager.Reset();

    g_pApplicationState->RegisterUpdateFunction([](ApplicationState& state)
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
    for (auto& [type, passes] : m_passes)
    {
        for (auto& pass : passes)
            pass->BuildPipelines();
    }
}

void PassManager::PreProcessMeshData(const stltype::vector<PassMeshData>& meshes, u32 lastFrame, u32 curFrame)
{
    auto& lastFrameCtx = m_frameResourceManager.GetFrameRendererContext(lastFrame);
    lastFrameCtx.pResourceManager = &m_resourceManager;
    for (auto& [type, passes] : m_passes)
    {
        for (auto& pass : passes)
        {
            for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
            {
                pass->RebuildInternalData(meshes, lastFrameCtx, i);
            }
            pass->NameResources(pass->GetName());
        }
    }
}

void PassManager::RecreateShadowMaps(u32 cascades, const mathstl::Vector2& extents)
{
    m_renderState.recreatedThisFrame = true;
    g_pApplicationState->RegisterUpdateFunction([](ApplicationState& state)
                                                { state.renderState.renderTargetsRecreatedThisFrame = true; });
    m_imguiRegistry.ReleaseShadowMapIdsForNextFrame();
    m_shadowMapManager.Recreate(cascades, extents, m_frameResourceManager);
    m_renderTargetManager.GetAttachments().directionalLightShadowMap = m_shadowMapManager.GetShadowMap();

    auto& shadowPasses = m_passes.at(PassType::Shadow);
    for (auto& pass : shadowPasses)
        if (auto* cp = dynamic_cast<CSMPass*>(pass.get()))
            cp->SetCascadeCount(cascades);
    if (m_passesInitialized)
    {
        m_imguiRegistry.RegisterShadowMapTextures(m_shadowMapManager.GetShadowMap());
    }
}
