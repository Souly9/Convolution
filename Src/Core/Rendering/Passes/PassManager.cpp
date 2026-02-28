#include "PassManager.h"
#include "Core/Global/FrameGlobals.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "ClusteredShading/ClusterDebugPass.h"
#include "ClusteredShading/LightGridComputePass.h"
#include "ClusteredShading/LightTransformComputePass.h"
#include "ClusteredShading/ClusterGeneratorComputePass.h"
#include "ClusteredShading/TileAssignmentComputePass.h"
#include "Compositing/CompositPass.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/GPUTimingQuery.h"
#include "Core/Rendering/Core/MaterialManager.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/Utils/DescriptorLayoutUtils.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"
#include "DebugShapePass.h"
#include "ImGuiPass.h"
#include "PreProcess/DepthPrePass.h"
#include "ScreenSpaceShadowPass.h"
#include "ShadowPass.h"
#include "StaticMeshPass.h"
#include <cstring>
#include <imgui/backends/imgui_impl_vulkan.h>

using namespace RenderPasses;

// Helper implementations to break up large functions
void PassManager::InitResourceManagerAndCallbacks()
{
    m_resourceManager.Init();
    g_pEventSystem->AddSceneLoadedEventCallback(
        [this](const SceneLoadedEventData&)
        {
            m_resourceManager.UploadSceneGeometry(g_pMeshManager->GetMeshes());
        });
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
    AddPass(PassType::UI, stltype::make_unique<RenderPasses::ImGuiPass>());
    AddPass(PassType::Composite, stltype::make_unique<RenderPasses::CompositPass>());
}

void PassManager::CreateUBOsAndMap()
{
    m_frameResourceManager.Init();
    m_frameResourceManager.CreatePassObjectsAndLayouts();
    m_frameResourceManager.CreateFrameRendererContexts(m_imageAvailableSemaphores, m_imageAvailableFences, m_renderFinishedFences);
}

void PassManager::InitPassesAndImGui()
{
    RecreateAllRenderResources();
    g_pQueueHandler->WaitForFences(~0u);
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
    depthRequest.usage = Usage::Sampled | Usage::DepthAttachment | Usage::TransferDst;
    depthRequest.isPersistent = true;
    depthRequest.AddName("Main Depth Buffer");
    
    m_pDepthTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(depthRequest));
    m_globalRendererAttachments.depthAttachment = DepthBufferAttachmentVulkan::Create(depthAttachmentInfo, m_pDepthTexture);

    // Make depth texture bindless
    m_depthBindlessHandle = g_pTexManager->MakeTextureBindless(depthRequest.handle, depthRequest.isPersistent);
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
            g_pTexManager->GetBindlessDescriptorSet();
        mainPassData.bufferDescriptors[UBO::DescriptorContentsType::BindlessImageArray] =
            g_pTexManager->GetBindlessImageDescriptorSet();
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
    for (const auto& passes : m_passes)
    {
        if (passes.first == PassType::PostProcess || passes.first == PassType::Composite)
            continue;
        for (const auto& pass : passes.second)
        {
            if (pass->WantsToRender())
                return true;
        }
    }
    return false;
}

// Timeline semaphore is used for pass group synchronization - no rebuild needed

void PassManager::PrepareMainPassDataForFrame(MainPassData& mainPassData, FrameRendererContext& ctx, u32 frameIdx)
{
    mainPassData.pResourceManager = &m_resourceManager;
    mainPassData.pGbuffer = &m_gbuffer;
    mainPassData.mainView.descriptorSet = ctx.sharedDataUBODescriptor;
    mainPassData.directionalLightShadowMap = m_globalRendererAttachments.directionalLightShadowMap;
    mainPassData.cascades = m_frameResourceManager.GetShadowMapState().cascadeCount;
    mainPassData.depthBufferBindlessHandle = m_depthBindlessHandle;
}

void PassManager::InitFrameContexts()
{
    const auto& indices = VkGlobals::GetQueueFamilyIndices();

    if (!m_graphicsFrameCtx.initialized)
    {
        m_graphicsFrameCtx.cmdPool = CommandPool::Create(indices.graphicsFamily.value());
        for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
        {
            m_graphicsFrameCtx.cmdBuffers[i] =
                m_graphicsFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
            m_graphicsFrameCtx.compositeCmdBuffers[i] =
                m_graphicsFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
            m_graphicsFrameCtx.depthPrePassCmdBuffers[i] =
                m_graphicsFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
        }
        m_graphicsFrameCtx.initialized = true;
    }

    if (!m_computeFrameCtx.initialized)
    {
        m_computeFrameCtx.cmdPool = CommandPool::Create(indices.computeFamily.value());
        for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
        {
            m_computeFrameCtx.cmdBuffers[i] = m_computeFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
            m_computeFrameCtx.sssComputeCmdBuffers[i] = m_computeFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
        }
        m_computeFrameCtx.initialized = true;
    }
}

void PassManager::RenderPassGroup(PassType groupType,
                                  const MainPassData& data,
                                  FrameRendererContext& ctx,
                                  CommandBuffer* pCmdBuffer)
{
    auto it = m_passes.find(groupType);
    if (it == m_passes.end() || it->second.empty())
        return;

    for (auto& pass : it->second)
    {
        if (pass->WantsToRender())
            pass->Render(data, ctx, pCmdBuffer);
    }
}

void PassManager::RenderAllPassGroups(const MainPassData& mainPassData,
                                      FrameRendererContext& ctx,
                                      Semaphore& imageAvailableSemaphore)
{
    // Reset GPU timing queries at the start of the frame
    // Reset on the CPU as we can guarantee the query pool is finished with the same frame idx 
    // (eg. if we're frame 0, we know the previous frame 0 already executed)
    if (m_gpuTimingQuery.IsEnabled())
    {
        m_gpuTimingQuery.ResetQueries(ctx.currentFrame);
    }

    stltype::vector<const Texture*> gbufferTextures = m_gbuffer.GetAllTextures();
    stltype::vector<const Texture*> allColorTextures = gbufferTextures;
    allColorTextures.push_back(ctx.pCurrentSwapchainTexture);

    CommandBuffer* pMainGraphicsWorkBuffer = m_graphicsFrameCtx.cmdBuffers[ctx.imageIdx];
    CommandBuffer* pComputeCmdBuffer = m_computeFrameCtx.cmdBuffers[ctx.imageIdx];
    CommandBuffer* pDepthWorkBuffer = m_graphicsFrameCtx.depthPrePassCmdBuffers[ctx.imageIdx];
    CommandBuffer* pSSSWorkBuffer = m_computeFrameCtx.sssComputeCmdBuffers[ctx.imageIdx];
    CommandBuffer* pCompositeCmdBuffer = m_graphicsFrameCtx.compositeCmdBuffers[ctx.imageIdx];
    (pMainGraphicsWorkBuffer)->ResetBuffer();
    (pComputeCmdBuffer)->ResetBuffer();
    (pDepthWorkBuffer)->ResetBuffer();
    (pSSSWorkBuffer)->ResetBuffer();
    (pCompositeCmdBuffer)->ResetBuffer();

    pMainGraphicsWorkBuffer->SetFrameIdx(ctx.currentFrame);
    pComputeCmdBuffer->SetFrameIdx(ctx.currentFrame);
    pDepthWorkBuffer->SetFrameIdx(ctx.currentFrame);
    pSSSWorkBuffer->SetFrameIdx(ctx.currentFrame);
    pCompositeCmdBuffer->SetFrameIdx(ctx.currentFrame);

    // Transition all color attachments and depth/shadow from UNDEFINED to their write layouts
    RecordInitialLayoutTransitions(pMainGraphicsWorkBuffer, allColorTextures);

    u64 computeSignalValue = ctx.nextComputeTimelineValue++;
    u64 depthPassSignalValue = ctx.nextTimelineValue++;
    u64 sssSignalValue = ctx.nextComputeTimelineValue++;
    u64 graphicsTimelineValue = ctx.nextTimelineValue++;

    // Dispatch early async compute work
    {
        RenderPassGroup(PassType::LightTransformCompute, mainPassData, ctx, pComputeCmdBuffer);

        GlobalBarrierCmd barrier1(SyncStages::COMPUTE_SHADER, SyncStages::COMPUTE_SHADER,
                                  VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        pComputeCmdBuffer->RecordCommand(barrier1);

        // Clear tile counters via DMA fill (no compute ALU cost)
        {
            constexpr u64 tileCountersOffset = offsetof(UBO::ViewSpaceLightsSSBO, tileCounters);
            BufferFillCmd fillCmd(&m_resourceManager.GetViewSpaceLightsSSBO(),
                                  tileCountersOffset,
                                  sizeof(UBO::ViewSpaceLightsSSBO) - tileCountersOffset,
                                  0);
            pComputeCmdBuffer->RecordCommand(fillCmd);

            GlobalBarrierCmd fillBarrier(SyncStages::TRANSFER, SyncStages::COMPUTE_SHADER,
                                         VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            pComputeCmdBuffer->RecordCommand(fillBarrier);
        }

        // Tile assignment + cluster gen can run in parallel (no data dependency)
        RenderPassGroup(PassType::TileAssignmentCompute, mainPassData, ctx, pComputeCmdBuffer);
        RenderPassGroup(PassType::ClusterGenCompute, mainPassData, ctx, pComputeCmdBuffer);

        GlobalBarrierCmd barrier2(SyncStages::COMPUTE_SHADER, SyncStages::COMPUTE_SHADER,
                                  VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        pComputeCmdBuffer->RecordCommand(barrier2);

        RenderPassGroup(PassType::EarlyAsyncCompute, mainPassData, ctx, pComputeCmdBuffer);

        (pComputeCmdBuffer)->AddTimelineSignal(&ctx.computeTimeline, computeSignalValue);
        pComputeCmdBuffer->SetSignalStages(SyncStages::COMPUTE_SHADER);
        
        pComputeCmdBuffer->Bake();
        g_pQueueHandler->SubmitCommandBufferThisFrame({pComputeCmdBuffer, QueueType::Compute, ctx.currentFrame});
    }

    // Manual dispatch Depth Pre-Pass (PreProcess group)
    {
        RecordInitialLayoutTransitions(pDepthWorkBuffer, allColorTextures);
        RenderPassGroup(PassType::PreProcess, mainPassData, ctx, pDepthWorkBuffer);
        RecordDepthToReadOnly(pDepthWorkBuffer);
        pDepthWorkBuffer->AddTimelineSignal(&ctx.frameTimeline, depthPassSignalValue);
        pDepthWorkBuffer->SetSignalStages(SyncStages::EARLY_FRAGMENT_TESTS | SyncStages::LATE_FRAGMENT_TESTS | SyncStages::COMPUTE_SHADER);
        pDepthWorkBuffer->Bake();
        g_pQueueHandler->SubmitCommandBufferThisFrame({pDepthWorkBuffer, QueueType::Graphics, ctx.currentFrame});
    }

    // Manual dispatch SSS (DepthReliantCompute group)
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

    // Record all graphics pass groups in schedule order (Starting from Main at stage 3)
    for (u32 stageIdx = 3; stageIdx < STAGE_COUNT - 2; ++stageIdx)
    {
        const auto& stage = PASS_SCHEDULE[stageIdx];

        CommandBuffer* pCurrentCmdBuffer = pMainGraphicsWorkBuffer;

        for (auto groupType : stage.groups)
            RenderPassGroup(groupType, mainPassData, ctx, pCurrentCmdBuffer);
    }

    // Setup semaphores
    // Main graphics work depends on Depth Pre-Pass
    pMainGraphicsWorkBuffer->AddTimelineWait(&ctx.frameTimeline, depthPassSignalValue);
    pMainGraphicsWorkBuffer->SetWaitStages(SyncStages::EARLY_FRAGMENT_TESTS | SyncStages::COLOR_ATTACHMENT_OUTPUT);

    pMainGraphicsWorkBuffer->AddTimelineSignal(&ctx.frameTimeline, graphicsTimelineValue);
    pMainGraphicsWorkBuffer->SetSignalStages(SyncStages::COLOR_ATTACHMENT_OUTPUT);

    pMainGraphicsWorkBuffer->Bake();
    g_pQueueHandler->SubmitCommandBufferThisFrame({pMainGraphicsWorkBuffer, QueueType::Graphics, ctx.currentFrame});

    // Composite and UI stage
    {
        RecordGBufferToShaderRead(pCompositeCmdBuffer, gbufferTextures);
        RenderPassGroup(PassType::Composite, mainPassData, ctx, pCompositeCmdBuffer);
        RenderPassGroup(PassType::UI, mainPassData, ctx, pCompositeCmdBuffer);
    }
    // Transition swapchain to present as the final command
    RecordSwapchainToPresent(pCompositeCmdBuffer);


    // compositeCmdBuffer will wait need swapchain, the async compute work and gbuffers hence we wait on everything
    // and will signal pPresentLayoutTransitionSignalSemaphore
    pCompositeCmdBuffer->AddWaitSemaphore(&imageAvailableSemaphore);
    pCompositeCmdBuffer->AddTimelineWait(&ctx.frameTimeline, graphicsTimelineValue);
    pCompositeCmdBuffer->AddTimelineWait(&ctx.computeTimeline, sssSignalValue);
    pCompositeCmdBuffer->SetWaitStages(SyncStages::COLOR_ATTACHMENT_OUTPUT | SyncStages::COMPUTE_SHADER);

    pCompositeCmdBuffer->AddSignalSemaphore(&ctx.pPresentLayoutTransitionSignalSemaphore);
    pCompositeCmdBuffer->SetSignalStages(SyncStages::COLOR_ATTACHMENT_OUTPUT);

    pCompositeCmdBuffer->Bake();
    g_pQueueHandler->SubmitCommandBufferThisFrame({pCompositeCmdBuffer, QueueType::Graphics, ctx.currentFrame});
    g_pQueueHandler->FlushGraphicsComputeBuffers();
}

void PassManager::RecordInitialLayoutTransitions(CommandBuffer* pCmdBuffer,
                                                 const stltype::vector<const Texture*>& allGbufferAndSwapchain)
{
    ImageLayoutTransitionCmd colorCmd(allGbufferAndSwapchain);
    colorCmd.oldLayout = ImageLayout::UNDEFINED;
    colorCmd.newLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(colorCmd, ImageLayout::UNDEFINED, ImageLayout::COLOR_ATTACHMENT_OPTIMAL);
    pCmdBuffer->RecordCommand(colorCmd);

    stltype::vector<const Texture*> depthTextures = {m_globalRendererAttachments.depthAttachment.GetTexture(),
                                                     m_globalRendererAttachments.directionalLightShadowMap.pTexture};
    ImageLayoutTransitionCmd depthCmd(depthTextures);
    depthCmd.oldLayout = ImageLayout::UNDEFINED;
    depthCmd.newLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(
        depthCmd, ImageLayout::UNDEFINED, ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    pCmdBuffer->RecordCommand(depthCmd);
}

void PassManager::RecordGBufferToShaderRead(CommandBuffer* pCmdBuffer,
                                            const stltype::vector<const Texture*>& gbufferTextures)
{
    ImageLayoutTransitionCmd colorCmd(gbufferTextures);
    colorCmd.oldLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    colorCmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(
        colorCmd, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(colorCmd);
    const auto* pShadowMap = m_globalRendererAttachments.directionalLightShadowMap.pTexture;
    if (pShadowMap == nullptr)
        return;

    stltype::vector<const Texture*> shadowTextures = {pShadowMap};
    ImageLayoutTransitionCmd shadowCmd(shadowTextures);
    shadowCmd.oldLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    shadowCmd.newLayout = ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(
        shadowCmd, ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(shadowCmd);
}

void PassManager::RecordDepthToReadOnly(CommandBuffer* pCmdBuffer)
{
    stltype::vector<const Texture*> depthTextures = {m_globalRendererAttachments.depthAttachment.GetTexture()};
    ImageLayoutTransitionCmd depthCmd(depthTextures);
    depthCmd.oldLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthCmd.newLayout = ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(
        depthCmd, ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(depthCmd);
}

void PassManager::RecordSSSOutputToGeneral(CommandBuffer* pCmdBuffer)
{
    if (!m_pScreenSpaceShadowTexture) return;
    ImageLayoutTransitionCmd outputToGeneralCmd(stltype::vector<const Texture*>{m_pScreenSpaceShadowTexture});
    outputToGeneralCmd.oldLayout = ImageLayout::UNDEFINED;
    outputToGeneralCmd.newLayout = ImageLayout::GENERAL;
    VkTextureManager::SetLayoutBarrierMasks(outputToGeneralCmd, ImageLayout::UNDEFINED, ImageLayout::GENERAL);
    pCmdBuffer->RecordCommand(outputToGeneralCmd);
}

void PassManager::RecordSSSOutputToShaderRead(CommandBuffer* pCmdBuffer)
{
    if (!m_pScreenSpaceShadowTexture) return;
    ImageLayoutTransitionCmd generalToShaderReadCmd(stltype::vector<const Texture*>{m_pScreenSpaceShadowTexture});
    generalToShaderReadCmd.oldLayout = ImageLayout::GENERAL;
    generalToShaderReadCmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(generalToShaderReadCmd, ImageLayout::GENERAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(generalToShaderReadCmd);
}



void PassManager::RecordSwapchainToPresent(CommandBuffer* pCmdBuffer)
{
    stltype::vector<const Texture*> textures = {
        m_frameResourceManager.GetFrameRendererContext(m_currentSwapChainIdx).pCurrentSwapchainTexture};
    ImageLayoutTransitionCmd cmd(textures);
    cmd.oldLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    cmd.newLayout = ImageLayout::PRESENT_SRC_KHR;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, ImageLayout::PRESENT_SRC_KHR);
    pCmdBuffer->RecordCommand(cmd);
}

void PassManager::Init()
{
    InitResourceManagerAndCallbacks();
    CreateUBOsAndMap(); // Reuses Initialize for everything
    InitFrameContexts();
    InitPassesAndImGui();

    // Removed caching arrays related to rendering loop

    // Initialize GPU timing query
    m_gpuTimingQuery.Init(128); // Support up to 128 passes

    // Wire timing query to all passes
    for (auto& [type, passes] : m_passes)
    {
        for (auto& pPass : passes)
            pPass->SetTimingQuery(&m_gpuTimingQuery);
    }
}

void PassManager::ExecutePasses(u32 frameIdx)
{
    auto& ctx = m_frameResourceManager.GetFrameRendererContext(m_currentSwapChainIdx);
    auto& mainPassData = m_mainPassData.at(m_currentSwapChainIdx);

    DEBUG_ASSERT(ctx.renderingFinishedSemaphore == &ctx.pPresentLayoutTransitionSignalSemaphore);
    DEBUG_ASSERT(ctx.renderingFinishedSemaphore->GetRef() != VK_NULL_HANDLE);

    auto& imageAvailableSemaphore = m_imageAvailableSemaphores.at(frameIdx);

    ctx.imageIdx = m_currentSwapChainIdx;
    ctx.currentFrame = frameIdx;
    ctx.pCurrentSwapchainTexture = &g_pTexManager->GetSwapChainTextures().at(m_currentSwapChainIdx);

    PrepareMainPassDataForFrame(mainPassData, ctx, frameIdx);

    RenderAllPassGroups(mainPassData, ctx, imageAvailableSemaphore);

    // UI and Composite always render, so we always present
    AsyncQueueHandler::PresentRequest presentRequest{.pWaitSemaphore = ctx.renderingFinishedSemaphore,
                                                     .swapChainImageIdx = m_currentSwapChainIdx,
                                                     .frameIdx = frameIdx};

    g_pQueueHandler->SubmitSwapchainPresentRequestForThisFrame(presentRequest);
    g_pQueueHandler->FlushGraphicsComputeBuffers();
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
    {
        passTimings.push_back({r.passName, r.gpuTimeMs, r.startMs, r.endMs, r.queueFamilyIndex, r.wasRun});
    }
    u64 totalVram = 0;
    u64 usedVram = 0;
    g_pGPUMemoryManager->GetVramStats(totalVram, usedVram);

    g_pApplicationState->RegisterUpdateFunction(
        [passTimings, totalTime, totalVram, usedVram](ApplicationState& state)
        {
            state.renderState.passTimings.clear();
            state.renderState.passTimings = passTimings;
            state.renderState.totalGPUTimeMs = totalTime;
            state.renderState.totalVramBytes = totalVram;
            state.renderState.usedVramBytes = usedVram;
        });
}

// Ensure destructor symbol exists
PassManager::~PassManager()
{
    m_gpuTimingQuery.Destroy();
}

// Restore functions removed during refactor
void PassManager::AddPass(PassType type, stltype::unique_ptr<ConvolutionRenderPass>&& pass)
{
    m_passes[type].push_back(std::move(pass));
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

void PassManager::SetLightDeltaForFrame(stltype::vector<LightDeltaUpdate>&& updates, bool dirLightDirty,
                                        const DirectionalRenderLight& dirLight, u32 frameIdx)
{
    m_frameResourceManager.SetLightDeltaForFrame(std::move(updates), dirLightDirty, dirLight, frameIdx);
}

void RenderPasses::PassManager::SetSharedData(RenderView&& mainView, u32 frameIdx)
{
    m_frameResourceManager.SetSharedData(std::move(mainView), frameIdx);
}

void PassManager::PreProcessDataForCurrentFrame(u32 frameIdx)
{
    m_frameResourceManager.PreProcessDataForCurrentFrame(frameIdx, m_currentSwapChainIdx, this);
}


void RenderPasses::PassManager::RecreateGbuffers(const mathstl::Vector2& resolution)
{
    auto& gbuffer = m_gbuffer;
    DynamicTextureRequest gbufferRequestBase{};
    gbufferRequestBase.extents = DirectX::XMUINT3(resolution.x, resolution.y, 1);
    gbufferRequestBase.hasMipMaps = false;
    gbufferRequestBase.usage = Usage::GBuffer;
    gbufferRequestBase.isPersistent = true;

    DynamicTextureRequest gbufferRequestPosition = gbufferRequestBase;
    gbufferRequestPosition.format = gbuffer.GetFormat(GBufferTextureType::GBufferAlbedo);
    gbufferRequestPosition.handle = g_pTexManager->GenerateHandle();
    gbufferRequestPosition.AddName("GBuffer Position");
    gbuffer.Set(GBufferTextureType::GBufferAlbedo,
                static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(gbufferRequestPosition)));

    DynamicTextureRequest gbufferRequestNormal = gbufferRequestBase;
    gbufferRequestNormal.format = gbuffer.GetFormat(GBufferTextureType::GBufferNormal);
    gbufferRequestNormal.handle = g_pTexManager->GenerateHandle();
    gbufferRequestNormal.AddName("GBuffer Normal");
    gbuffer.Set(GBufferTextureType::GBufferNormal,
                static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(gbufferRequestNormal)));

    DynamicTextureRequest gbufferRequestUV = gbufferRequestBase;
    gbufferRequestUV.format = gbuffer.GetFormat(GBufferTextureType::TexCoordMatData);
    gbufferRequestUV.handle = g_pTexManager->GenerateHandle();
    gbufferRequestUV.AddName("GBuffer UV Material Data");
    gbuffer.Set(GBufferTextureType::TexCoordMatData,
                static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(gbufferRequestUV)));
    g_pTexManager->MakeTextureBindless(gbufferRequestUV.handle, true);



    DynamicTextureRequest gbufferRequestDebug = gbufferRequestBase;
    gbufferRequestDebug.format = gbuffer.GetFormat(GBufferTextureType::GBufferDebug);
    gbufferRequestDebug.handle = g_pTexManager->GenerateHandle();
    gbufferRequestDebug.AddName("GBuffer Debug");
    gbuffer.Set(GBufferTextureType::GBufferDebug,
                static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(gbufferRequestDebug)));

    DynamicTextureRequest sssRequest = gbufferRequestBase;
    sssRequest.format = TexFormat::R8_UNORM;
    sssRequest.handle = g_pTexManager->GenerateHandle();
    sssRequest.AddName("Screen Space Shadows");
    sssRequest.usage = Usage::Storage | Usage::Sampled;

    if (m_pScreenSpaceShadowTexture)
    {
        g_pTexManager->FreeTexture(m_screenSpaceShadowsTextureHandle);
    }

    m_pScreenSpaceShadowTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(sssRequest));
    m_screenSpaceShadowsTextureHandle = sssRequest.handle;
    m_screenSpaceShadowBindlessHandle = g_pTexManager->MakeTextureBindless(sssRequest.handle, true);
    m_globalRendererAttachments.pScreenSpaceShadowTexture = m_pScreenSpaceShadowTexture;

    for (auto& passData : m_mainPassData)
    {
        passData.pScreenSpaceShadowTexture = m_pScreenSpaceShadowTexture;
        passData.screenSpaceShadows = m_screenSpaceShadowBindlessHandle;
    }

    stltype::vector<BindlessTextureHandle> gbufferHandles = {
        g_pTexManager->MakeTextureBindless(gbufferRequestPosition.handle, true),
        g_pTexManager->MakeTextureBindless(gbufferRequestNormal.handle, true),
        g_pTexManager->MakeTextureBindless(gbufferRequestUV.handle, true),
        g_pTexManager->MakeTextureBindless(gbufferRequestDebug.handle, true)};

    // Keep depth bound for composite/SSS paths that reconstruct position from scene depth.
    gbufferHandles.push_back(m_depthBindlessHandle);

    // Update gbuffer descriptor set
    memcpy(m_frameResourceManager.GetMappedGBufferPostProcessUBO(), &gbufferHandles[0], sizeof(BindlessTextureHandle) * gbufferHandles.size());
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; i++)
    {
        auto& ctx = m_frameResourceManager.GetFrameRendererContext(i);
        ctx.gbufferPostProcessDescriptor->WriteBufferUpdate(m_frameResourceManager.GetGBufferPostProcessUBO(), s_globalGbufferPostProcessUBOSlot);
    }
}



void RenderPasses::PassManager::BlockUntilPassesFinished(u32 frameIdx)
{
    ScopedZone("PassManager::Wait for previous frame render to finish");

    auto& imageAvailableFence = m_imageAvailableFences.at(frameIdx);
    auto& imageAvailableSemaphore = m_imageAvailableSemaphores.at(frameIdx);
    SRF::QueryImageForPresentationFromMainSwapchain<RenderAPI>(
        imageAvailableSemaphore, imageAvailableFence, m_currentSwapChainIdx);

    // Waiting here since we cant guarantee the presentation engine is done with the image until it is available for us
    // here (the fence from presentKHR is not enough) Reusing buffers or semaphores for this image before we are sure
    // it's not presented anymore will lead to synchronization errors
    imageAvailableFence.WaitFor();
    imageAvailableFence.Reset();

    // g_pQueueHandler->WaitForFences();
    //  vkQueueWaitIdle(VkGlobals::GetGraphicsQueue());
}

void RenderPasses::PassManager::RebuildPipelinesForAllPasses()
{
    ScopedZone("PassManager::RebuildPipelinesForAllPasses");

    if (g_pShaderManager->ReloadAllShaders() == false)
        return;

    for (auto& [type, passes] : m_passes)
    {
        for (auto& pass : passes)
        {
            pass->BuildPipelines();
        }
    }
}

void RenderPasses::PassManager::PreProcessMeshData(const stltype::vector<PassMeshData>& meshes,
                                                   u32 lastFrame,
                                                   u32 curFrame)
{
    ScopedZone("PassManager::PreProcessMeshData");

    auto& lastFrameCtx = m_frameResourceManager.GetFrameRendererContext(lastFrame);
    lastFrameCtx.pResourceManager = &m_resourceManager;
    for (auto& [type, passes] : m_passes)
    {
        for (auto& pass : passes)
        {
            pass->RebuildInternalData(meshes, lastFrameCtx, curFrame);
        }
    }
}

void RenderPasses::PassManager::RecreateShadowMaps(u32 cascades, const mathstl::Vector2& extents)
{
    ScopedZone("RecreateShadowMaps");

    auto& csm = m_globalRendererAttachments.directionalLightShadowMap;
    // Use cascadeViews.empty() as reliable check - pTexture may have garbage on first call
    if (!csm.cascadeViews.empty())
    {
        // Kinda okay because changing cascades can be slow!
        vkDeviceWaitIdle(VkGlobals::GetLogicalDevice());
        g_pTexManager->FreeTexture(csm.handle);
    }
    csm.cascades = cascades;
    csm.format = DEPTH_BUFFER_FORMAT;
    DynamicTextureRequest shadowMapInfo{};
    shadowMapInfo.AddName("Directional Light CSM");
    shadowMapInfo.isPersistent = true;
    csm.handle = shadowMapInfo.handle = g_pTexManager->GenerateHandle();
    shadowMapInfo.extents = DirectX::XMUINT3(extents.x, extents.y, cascades);
    shadowMapInfo.format = csm.format;
    shadowMapInfo.usage = Usage::ShadowMap;
    shadowMapInfo.samplerInfo.wrapU = TextureWrapMode::CLAMP_TO_BORDER;
    shadowMapInfo.samplerInfo.wrapV = TextureWrapMode::CLAMP_TO_BORDER;
    shadowMapInfo.samplerInfo.wrapW = TextureWrapMode::CLAMP_TO_BORDER;

    csm.pTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(shadowMapInfo));
    csm.bindlessHandle = g_pTexManager->MakeTextureBindless(shadowMapInfo.handle, true);

    for (auto& view : csm.cascadeViews)
    {
        if (view != VK_NULL_HANDLE)
        {
            vkDestroyImageView(VkGlobals::GetLogicalDevice(), view, nullptr);
        }
    }
    csm.cascadeViews.clear();
    csm.cascadeViews.resize(csm.cascades, VK_NULL_HANDLE);

    for (u32 i = 0; i < csm.cascades; ++i)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = csm.pTexture->GetImage();
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = Conv(csm.format);
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = i;
        viewInfo.subresourceRange.layerCount = 1;

        DEBUG_ASSERT(vkCreateImageView(VkGlobals::GetLogicalDevice(), &viewInfo, nullptr, &csm.cascadeViews[i]) ==
                     VK_SUCCESS);
    }

    // Update gbuffer descriptor set
    std::memcpy(m_frameResourceManager.GetMappedShadowMapUBO(), &csm.bindlessHandle, sizeof(BindlessTextureHandle));
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; i++)
    {
        auto& ctx = m_frameResourceManager.GetFrameRendererContext(i);
        ctx.gbufferPostProcessDescriptor->WriteBufferUpdate(m_frameResourceManager.GetShadowMapUBO(), s_shadowmapUBOBindingSlot);
    }

    // Notify CSM pass to rebuild pipeline with new cascade count
    auto it = m_passes.find(PassType::Shadow);
    if (it != m_passes.end())
    {
        for (auto& pass : it->second)
        {
            if (auto* csmPass = dynamic_cast<CSMPass*>(pass.get()))
            {
                csmPass->SetCascadeCount(cascades);
            }
        }
    }
}

void RenderPasses::PassManager::RegisterImGuiTextures()
{
    m_csmCascadeImGuiIDs.clear();

    const auto& csm = m_globalRendererAttachments.directionalLightShadowMap;
    if (csm.pTexture == nullptr || csm.cascadeViews.empty())
    {
        g_pApplicationState->RegisterUpdateFunction(
            [](ApplicationState& state) { state.renderState.csmCascadeImGuiIDs.clear(); });
        return;
    }

    m_csmCascadeImGuiIDs.reserve(csm.cascadeViews.size());
    for (const auto& cascadeView : csm.cascadeViews)
    {
        if (cascadeView == VK_NULL_HANDLE)
            continue;

        VkDescriptorSet id = ImGui_ImplVulkan_AddTexture(
            csm.pTexture->GetSampler(), cascadeView, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        m_csmCascadeImGuiIDs.push_back(reinterpret_cast<u64>(id));
    }

    g_pApplicationState->RegisterUpdateFunction(
        [cascadeIDs = m_csmCascadeImGuiIDs](ApplicationState& state) { state.renderState.csmCascadeImGuiIDs = cascadeIDs; });
}

void RenderPasses::PassManager::TransferPassData(const PassGeometryData& passData, u32 frameIdx)
{
    // Intentionally empty - placeholder for future transfer logic
    (void)passData;
    (void)frameIdx;
}

void RenderPasses::PassManager::UpdateImGuiGbufferTextures()
{
    m_gbufferImGuiIDs.clear();
    
    VkDescriptorSet idNormal =
        ImGui_ImplVulkan_AddTexture(m_gbuffer.Get(GBufferTextureType::GBufferNormal)->GetSampler(),
                                    m_gbuffer.Get(GBufferTextureType::GBufferNormal)->GetImageView(),
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorSet idAlbedo =
        ImGui_ImplVulkan_AddTexture(m_gbuffer.Get(GBufferTextureType::GBufferAlbedo)->GetSampler(),
                                    m_gbuffer.Get(GBufferTextureType::GBufferAlbedo)->GetImageView(),
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkDescriptorSet idDebug =
        ImGui_ImplVulkan_AddTexture(m_gbuffer.Get(GBufferTextureType::GBufferDebug)->GetSampler(),
                                    m_gbuffer.Get(GBufferTextureType::GBufferDebug)->GetImageView(),
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorSet idSSS = ImGui_ImplVulkan_AddTexture(
        m_pScreenSpaceShadowTexture->GetSampler(),
        m_pScreenSpaceShadowTexture->GetImageView(),
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    m_gbufferImGuiIDs.push_back(reinterpret_cast<u64>(idNormal));
    m_gbufferImGuiIDs.push_back(reinterpret_cast<u64>(idAlbedo));
    m_gbufferImGuiIDs.push_back(reinterpret_cast<u64>(idDebug));
    m_gbufferImGuiIDs.push_back(reinterpret_cast<u64>(idSSS));

    g_pApplicationState->RegisterUpdateFunction(
        [this](auto& state)
        {
            state.renderState.csmCascadeImGuiIDs = m_csmCascadeImGuiIDs;
            state.renderState.gbufferImGuiIDs = m_gbufferImGuiIDs;
        });
}
