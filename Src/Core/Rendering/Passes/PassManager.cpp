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
    auto& gbuffer = m_renderTargetManager.GetGBuffer();
    auto& attachments = m_renderTargetManager.GetAttachments();
    gbuffer.GetGeometryOutputTextures(gbufferTextures);

    stltype::fixed_vector<const Texture*, 16> allColorTextures;
    allColorTextures.assign(gbufferTextures.begin(), gbufferTextures.end());
    if (m_renderTargetManager.GetSMAAEdgesTexture() != nullptr)
    {
        allColorTextures.push_back(m_renderTargetManager.GetSMAAEdgesTexture());
    }
    if (m_renderTargetManager.GetSMAABlendTexture() != nullptr)
    {
        allColorTextures.push_back(m_renderTargetManager.GetSMAABlendTexture());
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

    auto pendingFlips = m_resourceManager.PopPendingVisibleInstanceIndices();

    u64 computeSignalValue = s_globalTimelineCounter.fetch_add(8);
    u64 depthPassSignalValue = computeSignalValue + 1;
    u64 sssSignalValue = computeSignalValue + 2;
    u64 mainPassSignalValue = computeSignalValue + 3;
    u64 rtComputeSignalValue = computeSignalValue + 4;
    u64 lightingSignalValue = computeSignalValue + 5;
    
    u64 graphicsTimelineValue = computeSignalValue + 7;
    const u64 submittedTransferValue = g_pQueueHandler->GetLastSubmittedValue(QueueType::Transfer);

    // Dispatch early async compute work
    {
        RenderPassGroup(PassType::LightTransformCompute, mainPassData, ctx, pComputeCmdBuffer);
        // Clearing previous frame tile buffer, just to make sure
        {
            Profiling::StartScope(
                pComputeCmdBuffer, &m_gpuTimingQuery, m_clearTileCountersTimingIndex, "Clearing Previous Tile Data");

            pComputeCmdBuffer->RecordCommand(GlobalBarrierCmd(SyncStages::COMPUTE_SHADER,
                                                              SyncStages::TRANSFER,
                                                              VK_ACCESS_SHADER_WRITE_BIT,
                                                              VK_ACCESS_TRANSFER_WRITE_BIT));

            constexpr u64 tileCountersOffset = MAX_SCENE_LIGHTS * sizeof(mathstl::Vector4);
            pComputeCmdBuffer->RecordCommand(BufferFillCmd(&m_resourceManager.GetViewSpaceLightsSSBO(),
                                                           tileCountersOffset,
                                                           UBO::ViewSpaceLightsSSBOSize - tileCountersOffset,
                                                           0));

            pComputeCmdBuffer->RecordCommand(GlobalBarrierCmd(SyncStages::TRANSFER | SyncStages::COMPUTE_SHADER,
                                                              SyncStages::COMPUTE_SHADER,
                                                              VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                                                              VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT));

            Profiling::EndScope(pComputeCmdBuffer, &m_gpuTimingQuery, m_clearTileCountersTimingIndex);
        }
        // Computing the tiles and their lights, split into three passes to alleviate register and memory pressure and
        // not check all lights for every cluster
        RenderPassGroup(PassType::TileAssignmentCompute, mainPassData, ctx, pComputeCmdBuffer);
        RenderPassGroup(PassType::ClusterGenCompute, mainPassData, ctx, pComputeCmdBuffer);

        pComputeCmdBuffer->RecordCommand(GlobalBarrierCmd(SyncStages::COMPUTE_SHADER,
                                                          SyncStages::COMPUTE_SHADER,
                                                          VK_ACCESS_SHADER_WRITE_BIT,
                                                          VK_ACCESS_SHADER_READ_BIT));

        RenderPassGroup(PassType::EarlyAsyncCompute, mainPassData, ctx, pComputeCmdBuffer);

        pComputeCmdBuffer->AddTimelineSignal(&ctx.computeTimeline, computeSignalValue);
        pComputeCmdBuffer->SetWaitStages(SyncStages::TRANSFER | SyncStages::COMPUTE_SHADER);
        pComputeCmdBuffer->SetSignalStages(SyncStages::COMPUTE_SHADER);
        pComputeCmdBuffer->Bake();
        g_pQueueHandler->SubmitCommandBufferThisFrame({pComputeCmdBuffer, QueueType::Compute, ctx.currentFrame});
    }

    // Depth Pre-Pass
    {
        if (!pendingFlips.empty())
        {
            Profiling::StartScope(
                pDepthWorkBuffer, &m_gpuTimingQuery, m_instanceBufferUpdateTimingIndex, "Instance Buffer Updates");

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

        m_transitionRecorder.RecordInitialLayoutTransitions(pDepthWorkBuffer, allColorTextures, attachments);
        RenderPassGroup(PassType::PreProcess, mainPassData, ctx, pDepthWorkBuffer);
        m_transitionRecorder.RecordDepthToReadOnly(pDepthWorkBuffer, attachments);
        pDepthWorkBuffer->AddTimelineSignal(&ctx.frameTimeline, depthPassSignalValue);
        pDepthWorkBuffer->SetWaitStages(SyncStages::TRANSFER | SyncStages::VERTEX_SHADER | SyncStages::FRAGMENT_SHADER |
                                        SyncStages::DEPTH_OUTPUT);
        pDepthWorkBuffer->SetSignalStages(SyncStages::DEPTH_OUTPUT);
        pDepthWorkBuffer->Bake();
        g_pQueueHandler->SubmitCommandBufferThisFrame({pDepthWorkBuffer, QueueType::Graphics, ctx.currentFrame});
    }

    // SSS (DepthReliantCompute)
    CommandBuffer* pSSSWorkBuffer = m_computeFrameCtx.sssComputeCmdBuffers[ctx.currentFrame];
    pSSSWorkBuffer->ResetBuffer();
    pSSSWorkBuffer->SetFrameIdx(ctx.currentFrame);
    {
        if (submittedTransferValue > 0)
            pSSSWorkBuffer->AddTimelineWait(g_pQueueHandler->GetTimelineSemaphore(QueueType::Transfer),
                                            submittedTransferValue);
        m_transitionRecorder.RecordSSSOutputToGeneral(pSSSWorkBuffer,
                                                      m_renderTargetManager.GetScreenSpaceShadowTexture());
        RenderPassGroup(PassType::DepthReliantCompute, mainPassData, ctx, pSSSWorkBuffer);
        m_transitionRecorder.RecordSSSOutputToShaderRead(pSSSWorkBuffer,
                                                         m_renderTargetManager.GetScreenSpaceShadowTexture());

        pSSSWorkBuffer->AddTimelineWait(&ctx.frameTimeline, depthPassSignalValue);
        pSSSWorkBuffer->SetWaitStages(SyncStages::TRANSFER | SyncStages::COMPUTE_SHADER);
        pSSSWorkBuffer->AddTimelineSignal(&ctx.computeTimeline, sssSignalValue);
        pSSSWorkBuffer->SetSignalStages(SyncStages::COMPUTE_SHADER);
        pSSSWorkBuffer->Bake();
        g_pQueueHandler->SubmitCommandBufferThisFrame({pSSSWorkBuffer, QueueType::Compute, ctx.currentFrame});
    }

    // Geometry Stage (Main -> Shadow)
    {
        pMainGraphicsWorkBuffer->AddTimelineWait(&ctx.frameTimeline, depthPassSignalValue);
        pMainGraphicsWorkBuffer->SetWaitStages(SyncStages::TRANSFER | SyncStages::VERTEX_SHADER |
                                               SyncStages::FRAGMENT_SHADER | SyncStages::EARLY_FRAGMENT_TESTS |
                                               SyncStages::COLOR_ATTACHMENT_OUTPUT);
        m_transitionRecorder.RecordPendingTextureUploadTransitions(pMainGraphicsWorkBuffer);
        m_transitionRecorder.RecordVelocityClear(pMainGraphicsWorkBuffer, gbuffer);
        RenderPassGroup(PassType::Main, mainPassData, ctx, pMainGraphicsWorkBuffer);
        RenderPassGroup(PassType::Debug, mainPassData, ctx, pMainGraphicsWorkBuffer);
        RenderPassGroup(PassType::Shadow, mainPassData, ctx, pMainGraphicsWorkBuffer);

        m_transitionRecorder.RecordGBufferToShaderRead(pMainGraphicsWorkBuffer, gbufferTextures, attachments);

        pMainGraphicsWorkBuffer->AddTimelineSignal(&ctx.frameTimeline, mainPassSignalValue);
        pMainGraphicsWorkBuffer->SetSignalStages(SyncStages::COLOR_ATTACHMENT_OUTPUT | SyncStages::DEPTH_OUTPUT);
        pMainGraphicsWorkBuffer->Bake();
        g_pQueueHandler->SubmitCommandBufferThisFrame({pMainGraphicsWorkBuffer, QueueType::Graphics, ctx.currentFrame});
    }

    // Lighting Stage + RTReflections
    CommandBuffer* pLightingWorkBuffer = m_graphicsFrameCtx.lightingCmdBuffers[ctx.currentFrame];
    pLightingWorkBuffer->ResetBuffer();
    pLightingWorkBuffer->SetFrameIdx(ctx.currentFrame);
    {
        pLightingWorkBuffer->AddTimelineWait(&ctx.frameTimeline, mainPassSignalValue);
        pLightingWorkBuffer->AddTimelineWait(&ctx.computeTimeline, sssSignalValue);
        if (submittedTransferValue > 0)
            pLightingWorkBuffer->AddTimelineWait(g_pQueueHandler->GetTimelineSemaphore(QueueType::Transfer),
                                                 submittedTransferValue);
        pLightingWorkBuffer->SetWaitStages(SyncStages::FRAGMENT_SHADER | SyncStages::COLOR_ATTACHMENT_OUTPUT |
                                           SyncStages::COMPUTE_SHADER);

        // Ensure Compute Queue shader writes (shadows/light grids) are visible to Graphics Queue shader reads
        pLightingWorkBuffer->RecordCommand(GlobalBarrierCmd(SyncStages::COMPUTE_SHADER,
                                                            SyncStages::COMPUTE_SHADER,
                                                            VK_ACCESS_SHADER_WRITE_BIT,
                                                            VK_ACCESS_SHADER_READ_BIT));

        // Transition GBufferThisFrameColor to GENERAL layout for compute shader write (discarding previous contents)
        m_transitionRecorder.RecordThisFrameColorToGeneralDiscard(pLightingWorkBuffer, gbuffer);

        RenderPassGroup(PassType::Lighting, mainPassData, ctx, pLightingWorkBuffer);

        // GBufferThisFrameColor to SHADER_READ so RTComposite can sample it
        m_transitionRecorder.RecordThisFrameColorToRead(pLightingWorkBuffer, gbuffer);

        // RT Reflections runs after lighting so it can sample GBuffer and depth
        m_rtResourceManager.RecordTransition(
            pLightingWorkBuffer, m_rtResourceManager.Get(RT::RTTextureType::Reflections), ImageLayout::GENERAL);
        RenderPassGroup(PassType::RTReflectionsCompute, mainPassData, ctx, pLightingWorkBuffer);
        m_rtResourceManager.RecordTransition(pLightingWorkBuffer,
                                             m_rtResourceManager.Get(RT::RTTextureType::Reflections),
                                             ImageLayout::SHADER_READ_ONLY_OPTIMAL);

        pLightingWorkBuffer->AddTimelineSignal(&ctx.frameTimeline, lightingSignalValue);
        pLightingWorkBuffer->SetSignalStages(SyncStages::COLOR_ATTACHMENT_OUTPUT | SyncStages::COMPUTE_SHADER);
        pLightingWorkBuffer->Bake();
        g_pQueueHandler->SubmitCommandBufferThisFrame({pLightingWorkBuffer, QueueType::Graphics, ctx.currentFrame});
    }

    // Async Compute RT Stage (RTAO, parallel with Lighting)
    CommandBuffer* pRTComputeCmdBuffer = m_computeFrameCtx.rtComputeCmdBuffers[ctx.currentFrame];
    pRTComputeCmdBuffer->ResetBuffer();
    pRTComputeCmdBuffer->SetFrameIdx(ctx.currentFrame);
    {
        pRTComputeCmdBuffer->AddTimelineWait(&ctx.frameTimeline, mainPassSignalValue);
        pRTComputeCmdBuffer->SetWaitStages(SyncStages::COMPUTE_SHADER);

        if (mainPassData.pRTAOTexture != nullptr)
        {
            m_rtResourceManager.RecordTransitionComputeOnly(
                pRTComputeCmdBuffer, m_rtResourceManager.Get(RT::RTTextureType::RTAO), ImageLayout::GENERAL);
        }

        RenderPassGroup(PassType::RTAOCompute, mainPassData, ctx, pRTComputeCmdBuffer);

        if (mainPassData.pRTAOTexture != nullptr)
        {
            m_rtResourceManager.RecordTransitionComputeOnly(pRTComputeCmdBuffer,
                                                            m_rtResourceManager.Get(RT::RTTextureType::RTAO),
                                                            ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        }

        pRTComputeCmdBuffer->AddTimelineSignal(&ctx.computeTimeline, rtComputeSignalValue);
        pRTComputeCmdBuffer->SetSignalStages(SyncStages::COMPUTE_SHADER);
        pRTComputeCmdBuffer->Bake();
        g_pQueueHandler->SubmitCommandBufferThisFrame({pRTComputeCmdBuffer, QueueType::Compute, ctx.currentFrame});
    }

    // Final Stage (RT Composite -> UI)
    CommandBuffer* pFinalWorkBuffer = m_graphicsFrameCtx.compositeCmdBuffers[ctx.currentFrame];
    pFinalWorkBuffer->ResetBuffer();
    pFinalWorkBuffer->SetFrameIdx(ctx.currentFrame);
    {
        pFinalWorkBuffer->AddTimelineWait(&ctx.frameTimeline, lightingSignalValue);
        pFinalWorkBuffer->AddTimelineWait(&ctx.computeTimeline, rtComputeSignalValue);
        pFinalWorkBuffer->AddWaitSemaphore(&imageAvailableSemaphore);
        pFinalWorkBuffer->SetWaitStages(SyncStages::FRAGMENT_SHADER | SyncStages::COMPUTE_SHADER |
                                        SyncStages::COLOR_ATTACHMENT_OUTPUT);

        // Ensure Compute Queue shader writes (RTAO/reflections) are visible to Graphics Queue shader reads
        pFinalWorkBuffer->RecordCommand(GlobalBarrierCmd(SyncStages::COMPUTE_SHADER,
                                                         SyncStages::FRAGMENT_SHADER | SyncStages::COMPUTE_SHADER,
                                                         VK_ACCESS_SHADER_WRITE_BIT,
                                                         VK_ACCESS_SHADER_READ_BIT));

        m_transitionRecorder.RecordSwapchainToAttachment(pFinalWorkBuffer, ctx.pCurrentSwapchainTexture);

        if (Nvidia::StreamlineManager::IsDLSSSupported())
        {
            m_transitionRecorder.RecordDLSSExposureUpdate(pFinalWorkBuffer,
                                                          m_renderTargetManager.GetDLSSExposureTexture(),
                                                          m_renderTargetManager.GetDLSSExposureStagingBuffer());
        }

        const auto& appRenderState = g_pApplicationState->GetCurrentApplicationState().renderState;
        const bool reflectionsEnabled =
            mathstl::isFlagSet(appRenderState.debugFlags, (u32)DebugFlags::RTEnabled) &&
            mathstl::isFlagSet(appRenderState.debugFlags, (u32)DebugFlags::RTReflectionsEnabled);
        const bool rtaoEnabled = mathstl::isFlagSet(appRenderState.debugFlags, (u32)DebugFlags::RTEnabled) &&
                                 mathstl::isFlagSet(appRenderState.debugFlags, (u32)DebugFlags::RTAOEnabled);
        const bool useRTReflections = reflectionsEnabled && mainPassData.pRTSceneManager != nullptr &&
                                      mainPassData.pRTSceneManager->HasReadyTLAS(m_currentSwapChainIdx);
        const bool useRTAO = rtaoEnabled && mainPassData.pRTSceneManager != nullptr &&
                             mainPassData.pRTSceneManager->HasReadyTLAS(m_currentSwapChainIdx);
        const bool useRT = useRTReflections || useRTAO;
        const bool useRayReconstruction = Nvidia::StreamlineManager::IsDLSSRRSupported() &&
                                          appRenderState.rt.reflectionsUseRayReconstruction && useRTReflections;

        // 1. RT Composite (combines Lighting, RTAO, Reflections)
        if (useRT && !useRayReconstruction)
        {
            m_rtResourceManager.RecordTransition(
                pFinalWorkBuffer, m_rtResourceManager.Get(RT::RTTextureType::Accumulation), ImageLayout::GENERAL);
            m_transitionRecorder.RecordThisFrameColorToGeneral(pFinalWorkBuffer, gbuffer);
            RenderPassGroup(PassType::RTComposite, mainPassData, ctx, pFinalWorkBuffer);
            m_transitionRecorder.RecordThisFrameColorFromGeneralToRead(pFinalWorkBuffer, gbuffer);
            m_rtResourceManager.RecordTransition(pFinalWorkBuffer,
                                                 m_rtResourceManager.Get(RT::RTTextureType::Accumulation),
                                                 ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        }

        const bool taaModeActive = appRenderState.aaType == AntialiasingType::TAA_SMAA;
        const bool smaaModeActive = appRenderState.aaType == AntialiasingType::SMAA;
        
        const bool seedHistoryFromCurrentColor = taaModeActive && appRenderState.taaSeedHistoryFromCurrentColor;
        const bool debugCopyCurrent =
            taaModeActive && appRenderState.taaDebugMode == static_cast<u32>(TAADebugMode::CurrentColor);
        const bool debugCopyHistory =
            taaModeActive && appRenderState.taaDebugMode == static_cast<u32>(TAADebugMode::HistoryColor);

        Texture* pCurrentSceneColor = gbuffer.Get(GBufferTextureType::GBufferThisFrameColor);

        if (useRayReconstruction)
        {
            // Skip other AA; DLSS-RR handles resolution, antialiasing, and denoising internally
        }
        else if (taaModeActive)
        {
            if (seedHistoryFromCurrentColor)
            {
                m_transitionRecorder.RecordCopyTextureToResolve(pFinalWorkBuffer, gbuffer, pCurrentSceneColor);
                g_pApplicationState->RegisterUpdateFunction(
                    [](ApplicationState& state) { state.renderState.taaSeedHistoryFromCurrentColor = false; });
            }
            else if (debugCopyCurrent)
            {
                m_transitionRecorder.RecordCopyTextureToResolve(pFinalWorkBuffer, gbuffer, pCurrentSceneColor);
            }
            else if (debugCopyHistory)
            {
                m_transitionRecorder.RecordCopyTextureToResolve(
                    pFinalWorkBuffer, gbuffer, gbuffer.Get(GBufferTextureType::GBufferLastFrameColor));
            }
            else
            {
                // Resolve is only a TAA target here. DLSS manages its own output transition/write path.
                m_transitionRecorder.RecordResolveToGeneral(pFinalWorkBuffer, gbuffer);
                RenderPassGroup(PassType::TAA, mainPassData, ctx, pFinalWorkBuffer);
                m_transitionRecorder.RecordResolveToRead(pFinalWorkBuffer, gbuffer);
                RenderPassGroup(PassType::SMAA, mainPassData, ctx, pFinalWorkBuffer);
            }
        }
        else if (smaaModeActive)
        {
            m_transitionRecorder.RecordCopyTextureToResolve(pFinalWorkBuffer, gbuffer, pCurrentSceneColor);
            RenderPassGroup(PassType::SMAA, mainPassData, ctx, pFinalWorkBuffer);
        }

        RenderPassGroup(PassType::DLSS, mainPassData, ctx, pFinalWorkBuffer);
        RenderPassGroup(PassType::DLSS_RR, mainPassData, ctx, pFinalWorkBuffer);
        RenderPassGroup(PassType::XeSS, mainPassData, ctx, pFinalWorkBuffer);

        RenderPassGroup(PassType::Composite, mainPassData, ctx, pFinalWorkBuffer);
        RenderPassGroup(PassType::UI, mainPassData, ctx, pFinalWorkBuffer);

        m_transitionRecorder.RecordSwapchainToPresent(pFinalWorkBuffer, ctx.pCurrentSwapchainTexture);

        if (Nvidia::StreamlineManager::IsAvailable())
        {
            ExecuteNativeCmd streamlineCmd{};
            const u32 frameSlot = ctx.currentFrame;
            const u32 frameIdx = ctx.currentFrame;
            Texture* pTex = ctx.pCurrentSwapchainTexture;
            streamlineCmd.callback = [frameIdx, frameSlot, pTex](void* pNativeCmdBuf) mutable
            {
                auto cmdBuffer = static_cast<VkCommandBuffer>(pNativeCmdBuf);
                sl::FrameToken* pFrameToken = nullptr;
                if (!Nvidia::StreamlineManager::GetFrameToken(frameIdx, pFrameToken) || !pFrameToken)
                    return;

                static struct
                {
                    sl::Resource res;
                    sl::ResourceTag tags[1];
                } s_slData[SWAPCHAIN_IMAGES];

                auto& data = s_slData[frameSlot];
                data.res = sl::Resource(sl::ResourceType::eTex2d,
                                        (void*)pTex->GetImage(),
                                        nullptr,
                                        (void*)pTex->GetImageView(),
                                        static_cast<uint32_t>(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR));
                data.res.width = pTex->GetInfo().extents.x;
                data.res.height = pTex->GetInfo().extents.y;
                data.res.nativeFormat = static_cast<uint32_t>(Conv(pTex->GetInfo().format));
                data.res.usage = Conv(pTex->GetInfo().usage);
                data.res.mipLevels = pTex->GetInfo().mipLevels > 0 ? pTex->GetInfo().mipLevels : 1u;
                data.res.arrayLayers = pTex->GetInfo().extents.z > 0 ? pTex->GetInfo().extents.z : 1u;
                data.res.flags = 0;

                data.tags[0] =
                    sl::ResourceTag(&data.res, sl::kBufferTypeBackbuffer, sl::ResourceLifecycle::eValidUntilPresent);

                sl::ViewportHandle viewportHandle(0);
                Nvidia::StreamlineManager::SetTagForFrame(*pFrameToken, viewportHandle, data.tags, 1, cmdBuffer);
            };
            pFinalWorkBuffer->RecordCommand(streamlineCmd);
        }

        pFinalWorkBuffer->AddSignalSemaphore(&ctx.pPresentLayoutTransitionSignalSemaphore);
        pFinalWorkBuffer->AddTimelineSignal(&ctx.frameTimeline, graphicsTimelineValue);
        pFinalWorkBuffer->SetSignalStages(SyncStages::ALL_COMMANDS);
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
    auto& gbuffer = m_renderTargetManager.GetGBuffer();
    UBO::GBufferPostProcessUBO gbufferUBO{};
    gbufferUBO.gbufferAlbedoIdx = gbuffer.GetHandle(GBufferTextureType::GBufferAlbedo);
    gbufferUBO.gbufferNormalIdx = gbuffer.GetHandle(GBufferTextureType::GBufferNormal);
    gbufferUBO.gbufferTexCoordMatIdx = gbuffer.GetHandle(GBufferTextureType::TexCoordMatData);
    gbufferUBO.gbufferDebugIdx = gbuffer.GetHandle(GBufferTextureType::GBufferDebug);
    gbufferUBO.gbufferVelocityIdx = gbuffer.GetHandle(GBufferTextureType::GBufferVelocity);
    gbufferUBO.lastFrameVelocityIdx = gbuffer.GetHandle(GBufferTextureType::GBufferLastFrameVelocity);
    gbufferUBO.depthBufferIdx = temporal.currentDepthHandle;
    gbufferUBO.lastFrameColorBufferIdx = temporal.historyColorHandle;
    gbufferUBO.lastFrameDepthIdx = temporal.historyDepthHandle;
    gbufferUBO.gbufferResolveIdx = temporal.resolveHandle;
    gbufferUBO.rtDebugViewIdx = data.rtDebugTextureHandle;
    gbufferUBO.rtReflectionsIdx = data.rtReflectionsTextureHandle;
    gbufferUBO.rtaoIdx = data.rtaoTextureHandle;
    gbufferUBO.deferredLightingColorIdx = gbuffer.GetHandle(GBufferTextureType::GBufferThisFrameColor);

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

    gbufferUBO.thisFrameColorBufferIdx = gbuffer.GetHandle(GBufferTextureType::GBufferThisFrameColor);

    Nvidia::StreamlineManager::SetUseRayReconstructionThisFrame(useRayReconstruction);

    gbufferUBO.finalTemporalColorBufferIdx = (((taaModeActive && !taaDebugOrSeed) || smaaModeActive) &&
                                              temporal.postAAColorHandle != 0 && !useRayReconstruction)
                                                 ? temporal.postAAColorHandle
                                                 : temporal.resolveHandle;

    if (useRayReconstruction)
    {
        gbufferUBO.thisFrameColorBufferIdx = temporal.resolveHandle;
    }
    else if (rtReflectionsRequested && rtState.reflectionsDebugMode == RTReflectionDebugMode::ReflectionsOnly)
    {
        gbufferUBO.thisFrameColorBufferIdx = data.rtReflectionsTextureHandle;
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

    if (g_pApplicationState->GetCurrentScene() == nullptr)
    {
        m_frameResourceManager.ClearGeometryCaches();
        m_resourceManager.ClearGeometryCaches();
        m_rtSceneManager.Reset();
    }

    m_frameResourceManager.PreProcessDataForCurrentFrame(frameIdx, jitterFrameNumber, m_currentSwapChainIdx, this);
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
