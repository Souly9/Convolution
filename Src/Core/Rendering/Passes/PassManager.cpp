#include "PassManager.h"
#include "ClusteredShading/ClusterDebugPass.h"
#include "ClusteredShading/LightGridComputePass.h"
#include "Compositing/CompositPass.h"
#include "Core/Rendering/Core/GPUTimingQuery.h"
#include "Core/Rendering/Core/MaterialManager.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/Utils/DescriptorLayoutUtils.h"
#include "DebugShapePass.h"
#include "ImGuiPass.h"
#include "PreProcess/DepthPrePass.h"
#include "ShadowPass.h"
#include "StaticMeshPass.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"
#include <cstring>
#include <imgui/backends/imgui_impl_vulkan.h>


using namespace RenderPasses;

// Helper implementations to break up large functions
void PassManager::InitResourceManagerAndCallbacks()
{
    m_resourceManager.Init();
    g_pEventSystem->AddSceneLoadedEventCallback(
        [this](const auto&) { m_resourceManager.UploadSceneGeometry(g_pMeshManager->GetMeshes()); });
    g_pEventSystem->AddShaderHotReloadEventCallback([this](const auto&) { RebuildPipelinesForAllPasses(); });

    // Create pass objects
    AddPass(PassType::PreProcess, stltype::make_unique<RenderPasses::LightGridComputePass>());
    AddPass(PassType::PreProcess, stltype::make_unique<RenderPasses::DepthPrePass>());
    AddPass(PassType::Main, stltype::make_unique<RenderPasses::StaticMainMeshPass>());
    AddPass(PassType::Main, stltype::make_unique<RenderPasses::DebugShapePass>());
    AddPass(PassType::Shadow, stltype::make_unique<RenderPasses::CSMPass>());
    AddPass(PassType::Debug, stltype::make_unique<RenderPasses::ClusterDebugPass>());
    AddPass(PassType::UI, stltype::make_unique<RenderPasses::ImGuiPass>());
    AddPass(PassType::Composite, stltype::make_unique<RenderPasses::CompositPass>());
}

void PassManager::CreatePassObjectsAndLayouts()
{
    DescriptorPoolCreateInfo info{};
    info.enableBindlessTextureDescriptors = false;
    info.enableStorageBufferDescriptors = true;
    m_descriptorPool.Create(info);

    m_lightClusterSSBOLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetForAll(
        {PipelineDescriptorLayout(UBO::BufferType::TileArraySSBO),
         PipelineDescriptorLayout(UBO::BufferType::LightUniformsUBO)});
    m_viewUBOLayout =
        DescriptorLaytoutUtils::CreateOneDescriptorSetForAll({PipelineDescriptorLayout(UBO::BufferType::View)});
    m_gbufferPostProcessLayout =
        DescriptorLaytoutUtils::CreateOneDescriptorSetForAll({PipelineDescriptorLayout(UBO::BufferType::GBufferUBO),
                                                              PipelineDescriptorLayout(UBO::BufferType::ShadowmapUBO)});
    m_shadowViewUBOLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetForAll(
        {PipelineDescriptorLayout(UBO::BufferType::ShadowmapViewUBO)});

    // Cluster grid
    m_clusterGridSSBOLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetForAll(
        {PipelineDescriptorLayout(UBO::BufferType::ClusterAABBsSSBO)});
}

void PassManager::CreateUBOsAndMap()
{
    u64 viewUBOSize = sizeof(UBO::ViewUBO);

    m_lightClusterSSBO = StorageBuffer(UBO::LightClusterSSBOSize, true);
    m_clusterGridSSBO = StorageBuffer(UBO::ClusterAABBSetSize, true);

    m_viewUBO = UniformBuffer(viewUBOSize);
    m_mappedViewUBOBuffer = m_viewUBO.MapMemory();
    m_lightUniformsUBO = UniformBuffer(sizeof(LightUniforms));
    m_gbufferPostProcessUBO = UniformBuffer(sizeof(UBO::GBufferPostProcessUBO));
    m_shadowMapUBO = UniformBuffer(sizeof(UBO::ShadowMapUBO));
    m_shadowViewUBO = UniformBuffer(sizeof(UBO::ShadowmapViewUBO));

    m_mappedLightUniformsUBO = m_lightUniformsUBO.MapMemory();
    m_mappedGBufferPostProcessUBO = m_gbufferPostProcessUBO.MapMemory();
    m_mappedShadowMapUBO = m_shadowMapUBO.MapMemory();
    m_mappedShadowViewUBO = m_shadowViewUBO.MapMemory();
}

void PassManager::CreateFrameRendererContexts()
{
    m_frameRendererContexts.resize(SWAPCHAIN_IMAGES);
    for (size_t i = 0; i < SWAPCHAIN_IMAGES; i++)
    {
        auto& frameContext = m_frameRendererContexts[i];
        frameContext.frameTimeline.Create(0);
        frameContext.computeTimeline.Create(0);
        frameContext.pPresentLayoutTransitionSignalSemaphore.Create();
        frameContext.shadowViewUBODescriptor = m_descriptorPool.CreateDescriptorSet(m_shadowViewUBOLayout.GetRef());
        frameContext.shadowViewUBODescriptor->WriteBufferUpdate(m_shadowViewUBO, s_shadowmapViewUBOBindingSlot);
        frameContext.mainViewUBODescriptor = m_descriptorPool.CreateDescriptorSet(m_viewUBOLayout.GetRef());
        frameContext.mainViewUBODescriptor->SetBindingSlot(s_viewBindingSlot);
        frameContext.mainViewUBODescriptor->WriteBufferUpdate(m_viewUBO, s_viewBindingSlot);

        frameContext.gbufferPostProcessDescriptor =
            m_descriptorPool.CreateDescriptorSet(m_gbufferPostProcessLayout.GetRef());
        frameContext.gbufferPostProcessDescriptor->WriteBufferUpdate(m_gbufferPostProcessUBO,
                                                                     s_globalGbufferPostProcessUBOSlot);
        frameContext.gbufferPostProcessDescriptor->WriteBufferUpdate(m_shadowMapUBO, s_shadowmapUBOBindingSlot);

        const auto numberString = stltype::to_string(i);
        frameContext.frameTimeline.SetName("Frame Timeline Semaphore " + numberString);
        frameContext.pPresentLayoutTransitionSignalSemaphore.SetName("Present Layout Transition Signal Semaphore " +
                                                                     numberString);
        m_imageAvailableSemaphores[i].Create();
        m_imageAvailableFences[i].Create(false);
        m_imageAvailableFences[i].SetName("Image Available Fence " + numberString);
        m_imageAvailableSemaphores[i].SetName("Image Available Semaphore " + numberString);
        // Create render finished fence as signaled so first frame doesn't wait
        m_renderFinishedFences[i].Create(true);
        m_renderFinishedFences[i].SetName("Render Finished Fence " + numberString);

        // Tile array data
        frameContext.tileArraySSBODescriptor = m_descriptorPool.CreateDescriptorSet(m_lightClusterSSBOLayout.GetRef());
        frameContext.tileArraySSBODescriptor->SetBindingSlot(s_tileArrayBindingSlot);
        frameContext.tileArraySSBODescriptor->WriteSSBOUpdate(m_lightClusterSSBO);
        frameContext.tileArraySSBODescriptor->WriteBufferUpdate(m_lightUniformsUBO, s_globalLightUniformsBindingSlot);

        // Cluster grid descriptor
        frameContext.clusterGridDescriptor = m_descriptorPool.CreateDescriptorSet(m_clusterGridSSBOLayout.GetRef());
        frameContext.clusterGridDescriptor->SetBindingSlot(s_clusterGridSSBOBindingSlot);
        frameContext.clusterGridDescriptor->WriteSSBOUpdate(m_clusterGridSSBO);

        // Point the rendering finished semaphore to the present transition semaphore for presentation sync
        frameContext.renderingFinishedSemaphore = &frameContext.pPresentLayoutTransitionSignalSemaphore;

        // Set shadow view UBO pointers for passes to use
        frameContext.pShadowViewUBO = &m_shadowViewUBO;
        frameContext.pMappedShadowViewUBO = m_mappedShadowViewUBO;

        UBO::ViewUBO view{};
        UpdateMainViewUBO((const void*)&view, sizeof(UBO::ViewUBO), i);
    }
}

void PassManager::InitPassesAndImGui()
{
    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;

    // Shadow maps
    {
        const auto csmCascades = renderState.directionalLightCascades;
        const auto csmResolution = renderState.csmResolution;
        RecreateShadowMaps(csmCascades, csmResolution);
        m_currentShadowMapState.cascadeCount = csmCascades;
        m_currentShadowMapState.shadowMapExtents = csmResolution;
    }

    // Moved RecreateGbuffers to after depth attachment creation
    // RecreateGbuffers(mathstl::Vector2(FrameGlobals::GetSwapChainExtent().x, FrameGlobals::GetSwapChainExtent().y));

    ColorAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.format = SWAPCHAIN_FORMAT;
    colorAttachmentInfo.finalLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    auto colorAttachment = ColorAttachment::Create(colorAttachmentInfo);

    DepthBufferAttachmentInfo depthAttachmentInfo{};
    depthAttachmentInfo.format = DEPTH_BUFFER_FORMAT;
    // create depth texture as in original implementation
    DynamicTextureRequest depthRequest{};
    depthRequest.handle = g_pTexManager->GenerateHandle();
    depthRequest.extents =
        DirectX::XMUINT3(FrameGlobals::GetSwapChainExtent().x, FrameGlobals::GetSwapChainExtent().y, 1);
    depthRequest.format = DEPTH_BUFFER_FORMAT;
    depthRequest.usage = Usage::Sampled | Usage::DepthAttachment | Usage::TransferDst;
    auto* pDepthTex = g_pTexManager->CreateTextureImmediate(depthRequest);
    auto depthAttachment = DepthBufferAttachmentVulkan::Create(depthAttachmentInfo, static_cast<Texture*>(pDepthTex));

    m_globalRendererAttachments.colorAttachments[ColorAttachmentType::GBufferColor].push_back(colorAttachment);
    m_globalRendererAttachments.depthAttachment = depthAttachment;

    // Make depth texture bindless
    m_depthBindlessHandle = g_pTexManager->MakeTextureBindless(depthRequest.handle);

    // Recreate GBuffers and update UBO (now that depth is ready)
    RecreateGbuffers(mathstl::Vector2(FrameGlobals::GetSwapChainExtent().x, FrameGlobals::GetSwapChainExtent().y));

    u32 idx = 0;
    for (auto& mainPassData : m_mainPassData)
    {
        mainPassData.bufferDescriptors[UBO::DescriptorContentsType::GlobalInstanceData] =
            m_resourceManager.GetInstanceSSBODescriptorSet(idx);
        mainPassData.bufferDescriptors[UBO::DescriptorContentsType::LightData] =
            m_frameRendererContexts[idx].tileArraySSBODescriptor;
        mainPassData.bufferDescriptors[UBO::DescriptorContentsType::GBuffer] =
            m_frameRendererContexts[idx].gbufferPostProcessDescriptor;
        mainPassData.bufferDescriptors[UBO::DescriptorContentsType::BindlessTextureArray] =
            g_pTexManager->GetBindlessDescriptorSet();
        mainPassData.bufferDescriptors[UBO::DescriptorContentsType::ClusterGrid] =
            m_frameRendererContexts[idx].clusterGridDescriptor;
        ++idx;
    }
    for (auto& [type, passes] : m_passes)
    {
        for (auto& pPass : passes)
        {
            pPass->Init(m_globalRendererAttachments, m_resourceManager);
        }
    }
    // ImGui textures
    VkDescriptorSet depthId = ImGui_ImplVulkan_AddTexture(
        pDepthTex->GetSampler(), pDepthTex->GetImageView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Register per-cascade CSM views (already created in RecreateShadowMaps or constructor)
    auto& csm = m_globalRendererAttachments.directionalLightShadowMap;
    stltype::vector<u64> cascadeImGuiIDs;
    cascadeImGuiIDs.reserve(csm.cascades);
    for (u32 i = 0; i < csm.cascades; ++i)
    {
        VkDescriptorSet cascadeDescriptor = ImGui_ImplVulkan_AddTexture(
            csm.pTexture->GetSampler(), csm.cascadeViews[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        cascadeImGuiIDs.push_back(reinterpret_cast<u64>(cascadeDescriptor));
    }

    VkDescriptorSet idNormal =
        ImGui_ImplVulkan_AddTexture(m_gbuffer.Get(GBufferTextureType::GBufferNormal)->GetSampler(),
                                    m_gbuffer.Get(GBufferTextureType::GBufferNormal)->GetImageView(),
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorSet idAlbedo =
        ImGui_ImplVulkan_AddTexture(m_gbuffer.Get(GBufferTextureType::GBufferAlbedo)->GetSampler(),
                                    m_gbuffer.Get(GBufferTextureType::GBufferAlbedo)->GetImageView(),
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorSet idUI = ImGui_ImplVulkan_AddTexture(m_gbuffer.Get(GBufferTextureType::GBufferUI)->GetSampler(),
                                                       m_gbuffer.Get(GBufferTextureType::GBufferUI)->GetImageView(),
                                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorSet idDebug =
        ImGui_ImplVulkan_AddTexture(m_gbuffer.Get(GBufferTextureType::GBufferDebug)->GetSampler(),
                                    m_gbuffer.Get(GBufferTextureType::GBufferDebug)->GetImageView(),
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    g_pApplicationState->RegisterUpdateFunction(
        [depthId, cascadeImGuiIDs, idNormal, idAlbedo, idUI, idDebug](auto& state)
        {
            state.renderState.csmCascadeImGuiIDs = cascadeImGuiIDs;
            state.renderState.depthbufferImGuiID = reinterpret_cast<u64>(depthId);
            state.renderState.gbufferImGuiIDs.clear();
            state.renderState.gbufferImGuiIDs.push_back(reinterpret_cast<u64>(idNormal));
            state.renderState.gbufferImGuiIDs.push_back(reinterpret_cast<u64>(idAlbedo));
            state.renderState.gbufferImGuiIDs.push_back(reinterpret_cast<u64>(idUI));
            state.renderState.gbufferImGuiIDs.push_back(reinterpret_cast<u64>(idDebug));
        });

    g_pQueueHandler->WaitForFences();
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
    mainPassData.mainView.descriptorSet = ctx.mainViewUBODescriptor;
    mainPassData.directionalLightShadowMap = m_globalRendererAttachments.directionalLightShadowMap;
    mainPassData.cascades = m_currentShadowMapState.cascadeCount;
    mainPassData.csmStepSize = g_pApplicationState->GetCurrentApplicationState().renderState.csmStepSize;
}


void PassManager::InitFrameContexts()
{
    const auto& indices = VkGlobals::GetQueueFamilyIndices();

    if (!m_graphicsFrameCtx.initialized)
    {
        m_graphicsFrameCtx.cmdPool = CommandPool::Create(indices.graphicsFamily.value());
        for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
            m_graphicsFrameCtx.cmdBuffers[i] = m_graphicsFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
        m_graphicsFrameCtx.initialized = true;
    }

    if (!m_computeFrameCtx.initialized)
    {
        m_computeFrameCtx.cmdPool = CommandPool::Create(indices.computeFamily.value());
        for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
            m_computeFrameCtx.cmdBuffers[i] = m_computeFrameCtx.cmdPool.CreateCommandBuffer(CommandBufferCreateInfo{});
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
    stltype::vector<const Texture*> gbufferTextures = m_gbuffer.GetAllTexturesWithoutUI();
    const auto* pUITexture = m_gbuffer.Get(GBufferTextureType::GBufferUI);
    stltype::vector<const Texture*> allColorTextures = gbufferTextures;
    allColorTextures.push_back(ctx.pCurrentSwapchainTexture);
    allColorTextures.push_back(pUITexture);

    CommandBuffer* cmdBuffer = m_graphicsFrameCtx.cmdBuffers[ctx.imageIdx];
    static_cast<CBufferVulkan*>(cmdBuffer)->ResetBuffer();

    // Reset GPU timing queries at the start of the frame
    if (m_gpuTimingQuery.IsEnabled())
        m_gpuTimingQuery.ResetQueries(cmdBuffer, ctx.currentFrame);

    // Transition all color attachments and depth/shadow from UNDEFINED to their write layouts
    RecordInitialLayoutTransitions(cmdBuffer, allColorTextures);

    // Record all graphics pass groups in schedule order
    for (u32 stageIdx = 0; stageIdx < STAGE_COUNT; ++stageIdx)
    {
        const auto& stage = PASS_SCHEDULE[stageIdx];

        bool isComputeStage = false;
        for (auto groupType : stage.groups)
            if (groupType == PassType::AsyncCompute)
            {
                isComputeStage = true;
                break;
            }

        if (isComputeStage)
            continue;

        bool hasGeometry = false;
        bool hasUI = false;
        for (auto groupType : stage.groups)
        {
            if (groupType == PassType::Main || groupType == PassType::Shadow || groupType == PassType::Debug)
                hasGeometry = true;
            if (groupType == PassType::UI)
                hasUI = true;
        }

        for (auto groupType : stage.groups)
            RenderPassGroup(groupType, mainPassData, ctx, cmdBuffer);

        // Insert barriers between geometry passes and the composite/UI that reads them
        if (hasGeometry)
        {
            RecordGBufferToShaderRead(cmdBuffer, gbufferTextures);
        }
        if (hasUI)
        {
            RecordUIToShaderRead(cmdBuffer, pUITexture);
        }
    }

    // Transition swapchain to present as the final command
    RecordSwapchainToPresent(cmdBuffer);

    cmdBuffer->Bake();

    // Single graphics submit: wait on image available (binary), signal present semaphore (binary)
    static_cast<CBufferVulkan*>(cmdBuffer)->AddWaitSemaphore(&imageAvailableSemaphore);
    static_cast<CBufferVulkan*>(cmdBuffer)->AddSignalSemaphore(&ctx.pPresentLayoutTransitionSignalSemaphore);
    cmdBuffer->SetWaitStages(SyncStages::COLOR_ATTACHMENT_OUTPUT);
    cmdBuffer->SetSignalStages(SyncStages::COLOR_ATTACHMENT_OUTPUT);

    g_pQueueHandler->SubmitCommandBufferThisFrame({cmdBuffer, QueueType::Graphics});
}


void PassManager::RecordInitialLayoutTransitions(CommandBuffer* pCmdBuffer,
                                                 const stltype::vector<const Texture*>& allGbufferAndSwapchain)
{
    ImageLayoutTransitionCmd colorCmd(allGbufferAndSwapchain);
    colorCmd.oldLayout = ImageLayout::UNDEFINED;
    colorCmd.newLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(colorCmd, ImageLayout::UNDEFINED, ImageLayout::COLOR_ATTACHMENT_OPTIMAL);
    pCmdBuffer->RecordCommand(colorCmd);

    stltype::vector<const Texture*> depthTextures = {
        m_globalRendererAttachments.depthAttachment.GetTexture(),
        m_globalRendererAttachments.directionalLightShadowMap.pTexture};
    ImageLayoutTransitionCmd depthCmd(depthTextures);
    depthCmd.oldLayout = ImageLayout::UNDEFINED;
    depthCmd.newLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(depthCmd, ImageLayout::UNDEFINED, ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    pCmdBuffer->RecordCommand(depthCmd);
}

void PassManager::RecordGBufferToShaderRead(CommandBuffer* pCmdBuffer,
                                             const stltype::vector<const Texture*>& gbufferTextures)
{
    ImageLayoutTransitionCmd colorCmd(gbufferTextures);
    colorCmd.oldLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    colorCmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(colorCmd, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(colorCmd);

    stltype::vector<const Texture*> depthTextures = {
        m_globalRendererAttachments.directionalLightShadowMap.pTexture,
        m_globalRendererAttachments.depthAttachment.GetTexture()};
    ImageLayoutTransitionCmd depthCmd(depthTextures);
    depthCmd.oldLayout = ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthCmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(depthCmd, ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(depthCmd);
}

void PassManager::RecordUIToShaderRead(CommandBuffer* pCmdBuffer, const Texture* pUITexture)
{
    stltype::vector<const Texture*> textures = {pUITexture};
    ImageLayoutTransitionCmd cmd(textures);
    cmd.oldLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    cmd.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    pCmdBuffer->RecordCommand(cmd);
}

void PassManager::RecordSwapchainToPresent(CommandBuffer* pCmdBuffer)
{
    stltype::vector<const Texture*> textures = {m_frameRendererContexts[m_currentSwapChainIdx].pCurrentSwapchainTexture};
    ImageLayoutTransitionCmd cmd(textures);
    cmd.oldLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
    cmd.newLayout = ImageLayout::PRESENT_SRC_KHR;
    VkTextureManager::SetLayoutBarrierMasks(cmd, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, ImageLayout::PRESENT_SRC_KHR);
    pCmdBuffer->RecordCommand(cmd);
}

void PassManager::Init()
{
    InitResourceManagerAndCallbacks();
    CreatePassObjectsAndLayouts();
    CreateUBOsAndMap();
    CreateFrameRendererContexts();
    InitFrameContexts();
    InitPassesAndImGui();

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
    auto& mainPassData = m_mainPassData.at(frameIdx);

    // UI and Composite always render, so we always have something to present
    auto& ctx = m_frameRendererContexts.at(m_currentSwapChainIdx);

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
                                                     .swapChainImageIdx = m_currentSwapChainIdx};

    g_pQueueHandler->SubmitSwapchainPresentRequestForThisFrame(presentRequest);
    g_pQueueHandler->DispatchAllRequests();
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
    g_pApplicationState->RegisterUpdateFunction(
        [passTimings, totalTime](ApplicationState& state)
        {
            state.renderState.passTimings.clear();
            state.renderState.passTimings = passTimings;
            state.renderState.totalGPUTimeMs = totalTime;
        });

    m_gpuTimingQuery.ClearRunFlags();
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
    // DEBUG_ASSERT(frameIdx == FrameGlobals::GetFrameNumber());
    m_passDataMutex.lock();
    m_dataToBePreProcessed.entityMeshData = std::move(data);
    m_dataToBePreProcessed.frameIdx = frameIdx;
    m_passDataMutex.unlock();
}

void PassManager::SetEntityTransformDataForFrame(TransformSystemData&& data, u32 frameIdx)
{
    // DEBUG_ASSERT(frameIdx == FrameGlobals::GetFrameNumber());
    m_passDataMutex.lock();
    m_dataToBePreProcessed.entityTransformData = std::move(data);
    m_dataToBePreProcessed.frameIdx = frameIdx;
    m_passDataMutex.unlock();
}

void PassManager::SetLightDataForFrame(PointLightVector&& data, DirLightVector&& dirLights, u32 frameIdx)
{
    // DEBUG_ASSERT(frameIdx == FrameGlobals::GetFrameNumber());
    m_passDataMutex.lock();
    m_dataToBePreProcessed.lightVector = data;
    m_dataToBePreProcessed.dirLightVector = dirLights;
    m_dataToBePreProcessed.frameIdx = frameIdx;
    m_passDataMutex.unlock();
}

void PassManager::SetMainViewData(UBO::ViewUBO&& viewUBO, f32 zNear, f32 zFar, u32 frameIdx)
{
    m_passDataMutex.lock();
    m_dataToBePreProcessed.mainViewUBO = std::move(viewUBO);
    m_dataToBePreProcessed.zNear = zNear;
    m_dataToBePreProcessed.zFar = zFar;
    m_dataToBePreProcessed.frameIdx = frameIdx;
    m_passDataMutex.unlock();
}

void PassManager::PreProcessDataForCurrentFrame(u32 frameIdx)
{
    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    // Recreate shadow maps
    {
        const auto csmCascades = renderState.directionalLightCascades;
        const auto csmResolution = renderState.csmResolution;
        if (m_currentShadowMapState.cascadeCount != csmCascades ||
            m_currentShadowMapState.shadowMapExtents.x != csmResolution.x ||
            m_currentShadowMapState.shadowMapExtents.y != csmResolution.y)
        {
            RecreateShadowMaps(csmCascades, csmResolution);
            m_currentShadowMapState.cascadeCount = csmCascades;
            m_currentShadowMapState.shadowMapExtents = csmResolution;
        }
    }
    if (m_needsToPropagateMainDataUpdate && m_dataToBePreProcessed.IsEmpty())
    {
        u32 frameToPropagate = m_frameIdxToPropagate;
        u32 targetFrame = FrameGlobals::GetPreviousFrameNumber(m_frameIdxToPropagate);
        m_needsToPropagateMainDataUpdate = false;
        // Use m_currentSwapChainIdx because that's the context we are preparing for
        auto& ctx = m_frameRendererContexts[m_currentSwapChainIdx];

        // Also update zNear/zFar for the target frame if we are propagating
        ctx.zNear = m_dataToBePreProcessed.zNear;
        ctx.zFar = m_dataToBePreProcessed.zFar;

        m_resourceManager.WriteInstanceSSBODescriptorUpdate(m_currentSwapChainIdx);

        ctx.mainViewUBODescriptor->WriteBufferUpdate(m_viewUBO);
        ctx.tileArraySSBODescriptor->WriteSSBOUpdate(m_lightClusterSSBO);
        ctx.tileArraySSBODescriptor->WriteBufferUpdate(m_lightUniformsUBO, s_globalLightUniformsBindingSlot);
    }

    if (g_pMaterialManager->IsBufferDirty())
    {
        m_resourceManager.UpdateGlobalMaterialBuffer(g_pMaterialManager->GetMaterialBuffer(), m_currentSwapChainIdx);
        m_needsToPropagateMainDataUpdate = true;
        m_frameIdxToPropagate = frameIdx;
        g_pMaterialManager->MarkBufferUploaded();
    }

    if (m_dataToBePreProcessed.IsEmpty() == false)
    {
        DEBUG_ASSERT(m_dataToBePreProcessed.IsValid());
        PassGeometryData passData{};

        stltype::vector<DirectX::XMFLOAT4X4> transformSSBO(MAX_ENTITIES);
        stltype::vector<AABB> sceneAABBs(MAX_ENTITIES);

        if (m_dataToBePreProcessed.entityMeshData.empty() == false)
        {
            stltype::hash_map<u64, u32> entityToMeshIdx;
            entityToMeshIdx.reserve(m_dataToBePreProcessed.entityMeshData.size());
            u32 meshIdx = 0;

            for (const auto& data : m_dataToBePreProcessed.entityTransformData)
            {
                const auto& entityID = data.first;
                DEBUG_ASSERT(entityToMeshIdx.find(entityID) == entityToMeshIdx.end());
                entityToMeshIdx.insert({entityID, meshIdx});
                ++meshIdx;
            }

            passData.staticMeshPassData.reserve(m_dataToBePreProcessed.entityMeshData.size());
            m_entityToTransformUBOIdx = entityToMeshIdx;
            for (const auto& meshDataVecPair : m_dataToBePreProcessed.entityMeshData)
            {
                const auto& entityID = meshDataVecPair.first;
                for (const auto& meshData : meshDataVecPair.second)
                {
                    DEBUG_ASSERT(m_entityToTransformUBOIdx.find(entityID) != m_entityToTransformUBOIdx.end());
                    const auto& bufferIndex = m_entityToTransformUBOIdx[entityID];
                    passData.staticMeshPassData.emplace_back(meshData, bufferIndex);
                }
            }

            // Compare to current state
            bool needsRebuild = false;
            // Refactor the preprocessmeshdata to also hand over current state and check for differences
            if (m_currentPassGeometryState.staticMeshPassData.size() == passData.staticMeshPassData.size())
            {
                for (u32 i = 0; i < m_currentPassGeometryState.staticMeshPassData.size(); ++i)
                {
                    const auto& currentMeshData = m_currentPassGeometryState.staticMeshPassData[i].meshData;
                    const auto& newMeshData = passData.staticMeshPassData[i].meshData;
                    if (newMeshData.DidGeometryChange(currentMeshData))
                    {
                        needsRebuild = true;
                        break;
                    }
                }
            }
            else
            {
                needsRebuild = true;
            }
            m_currentPassGeometryState = passData;
            if (needsRebuild)
            {
                m_resourceManager.UpdateInstanceDataSSBO(passData.staticMeshPassData, m_currentSwapChainIdx);
                PreProcessMeshData(
                    passData.staticMeshPassData, FrameGlobals::GetPreviousFrameNumber(frameIdx), frameIdx);
            }
            TransferPassData(std::move(passData), m_dataToBePreProcessed.frameIdx);
        }

        for (const auto& data : m_dataToBePreProcessed.entityTransformData)
        {
            const auto& entityID = data.first;
            transformSSBO[m_entityToTransformUBOIdx[entityID]] = data.second;
            // Also populate AABBs
            AABB aabb{};
            if (m_dataToBePreProcessed.entityMeshData.find(entityID) != m_dataToBePreProcessed.entityMeshData.end())
            {
                const auto& meshDataVec = m_dataToBePreProcessed.entityMeshData.at(entityID);
                if (!meshDataVec.empty())
                {
                    aabb = meshDataVec[0].aabb;
                }
            }
            sceneAABBs[m_entityToTransformUBOIdx[entityID]] = aabb;
        }
        m_resourceManager.UpdateTransformBuffer(transformSSBO, m_currentSwapChainIdx);
        m_resourceManager.UpdateSceneAABBBuffer(sceneAABBs, m_currentSwapChainIdx);

        if (m_dataToBePreProcessed.mainViewUBO.has_value())
        {
            UpdateMainViewUBO(&m_dataToBePreProcessed.mainViewUBO.value(), sizeof(UBO::ViewUBO), m_currentSwapChainIdx);
        }

        // Set zNear/zFar for this frame's context (needed by shadow pass)
        auto& ctx = m_frameRendererContexts[m_currentSwapChainIdx];
        ctx.zNear = m_dataToBePreProcessed.zNear;
        ctx.zFar = m_dataToBePreProcessed.zFar;

        // Set the camera view matrix for shadow cascade computation
        if (m_dataToBePreProcessed.mainViewUBO.has_value())
        {
            m_mainPassData[m_currentSwapChainIdx].mainCamViewMatrix = m_dataToBePreProcessed.mainViewUBO->view;
            m_mainPassData[m_currentSwapChainIdx].mainCamInvViewProj =
                m_dataToBePreProcessed.mainViewUBO->viewProjection.Invert();
        }

        if (m_dataToBePreProcessed.lightVector.empty() == false ||
            m_dataToBePreProcessed.dirLightVector.empty() == false)
        {
            // Light data - populate LightClusterSSBO
            auto& lightCluster = m_lightCluster;
            lightCluster.numLights = 0;

            for (const auto& light : m_dataToBePreProcessed.lightVector)
            {
                if (lightCluster.numLights < MAX_SCENE_LIGHTS)
                {
                    lightCluster.lights[lightCluster.numLights++] = light;
                }
            }
            if (m_dataToBePreProcessed.dirLightVector.empty() == false)
            {
                const auto& dirLight = m_dataToBePreProcessed.dirLightVector[0];
                lightCluster.dirLight.direction =
                    mathstl::Vector4(dirLight.direction.x, dirLight.direction.y, dirLight.direction.z, 1);
                lightCluster.dirLight.direction.Normalize();
                lightCluster.dirLight.color =
                    mathstl::Vector4(dirLight.color.x, dirLight.color.y, dirLight.color.z, dirLight.color.w);
            }
            UpdateLightClusterSSBO(lightCluster, m_currentSwapChainIdx);

            // Directional lights
            {
                const auto& dirLights = m_dataToBePreProcessed.dirLightVector;
                m_mainPassData.at(m_currentSwapChainIdx).csmViews.clear();
                for (const auto& dirLight : dirLights)
                {
                    auto& dirLightView = m_mainPassData.at(m_currentSwapChainIdx).csmViews.emplace_back();
                    dirLightView.dir =
                        mathstl::Vector3(dirLight.direction.x, dirLight.direction.y, dirLight.direction.z);
                    dirLightView.cascades = 1; // Force Single Shadow Map
                }
            }
        }

        // Clear
        m_needsToPropagateMainDataUpdate = true;
        m_frameIdxToPropagate = m_currentSwapChainIdx;
        m_dataToBePreProcessed.Clear();
        g_pQueueHandler->DispatchAllRequests();
    }

    // Light uniforms (updated every frame for camera pos, exposure, ambient, etc.)
    {
        auto& mainPassData = m_mainPassData.at(m_currentSwapChainIdx);
        const auto camEnt = g_pApplicationState->GetCurrentApplicationState().mainCameraEntity;
        const ECS::Components::Camera* camComp = g_pEntityManager->GetComponent<ECS::Components::Camera>(camEnt);

        // Skip if camera component doesn't exist yet
        if (!camComp)
            return;

        LightUniforms data;

        // Get camera position from ECS Transform component
        const ECS::Components::Transform* camTransform =
            g_pEntityManager->GetComponent<ECS::Components::Transform>(camEnt);
        mathstl::Vector3 camPos = camTransform ? camTransform->position : mathstl::Vector3::Zero;

        data.CameraPos = mathstl::Vector4(camPos.x, camPos.y, camPos.z, 1);
        data.LightGlobals = mathstl::Vector4(renderState.exposure,
                                             static_cast<f32>(renderState.toneMapperType),
                                             renderState.ambientIntensity,
                                             static_cast<f32>(renderState.debugViewMode));

        // Cluster parameters
        // Assume zNear/zFar from camera
        float zNear = camComp->zNear;
        float zFar = camComp->zFar;
        u32 sliceCount = renderState.clusterCount.z;

        float logRatio = std::log(zFar / zNear);
        float scale = (float)sliceCount / logRatio;
        float bias = -std::log(zNear) * scale;

        data.ClusterValues = mathstl::Vector4(scale, bias, (float)sliceCount, renderState.shadowsEnabled ? 1.0f : 0.0f);
        data.ClusterSize = mathstl::Vector4((float)renderState.clusterCount.x,
                                            (float)renderState.clusterCount.y,
                                            (float)renderState.clusterCount.z,
                                            0.0f); // TODO: Tile size in pixels if needed
        data.GT7Params = mathstl::Vector4(renderState.gt7PaperWhite, renderState.gt7ReferenceLuminance, 0.0f, 0.0f);

        mainPassData.mainView.viewport =
            RenderViewUtils::CreateViewportFromData(FrameGlobals::GetSwapChainExtent(), camComp->zNear, camComp->zFar);
        mainPassData.mainView.fov = camComp->fov;

        auto& ctx = m_frameRendererContexts[m_currentSwapChainIdx];
        if (m_dataToBePreProcessed.frameIdx == frameIdx)
        {
            ctx.zNear = m_dataToBePreProcessed.zNear;
            ctx.zFar = m_dataToBePreProcessed.zFar;
        }

        auto pDescriptor = m_frameRendererContexts[m_currentSwapChainIdx].tileArraySSBODescriptor;
        auto mappedBuffer = m_mappedLightUniformsUBO;
        memcpy(mappedBuffer, &data, m_lightUniformsUBO.GetInfo().size);
        pDescriptor->WriteBufferUpdate(m_lightUniformsUBO, s_globalLightUniformsBindingSlot);
    }
}

void RenderPasses::PassManager::RecreateGbuffers(const mathstl::Vector2& resolution)
{
    auto& gbuffer = m_gbuffer;
    DynamicTextureRequest gbufferRequestBase{};
    gbufferRequestBase.extents = DirectX::XMUINT3(resolution.x, resolution.y, 1);
    gbufferRequestBase.hasMipMaps = false;
    gbufferRequestBase.usage = Usage::GBuffer;

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
    g_pTexManager->MakeTextureBindless(gbufferRequestUV.handle);

    DynamicTextureRequest gbufferRequestUI = gbufferRequestBase;
    gbufferRequestUI.format = gbuffer.GetFormat(GBufferTextureType::GBufferUI);
    gbufferRequestUI.handle = g_pTexManager->GenerateHandle();
    gbufferRequestUI.AddName("GBuffer UI");
    gbuffer.Set(GBufferTextureType::GBufferUI,
                static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(gbufferRequestUI)));

    DynamicTextureRequest gbufferRequestDebug = gbufferRequestBase;
    gbufferRequestDebug.format = gbuffer.GetFormat(GBufferTextureType::GBufferDebug);
    gbufferRequestDebug.handle = g_pTexManager->GenerateHandle();
    gbufferRequestDebug.AddName("GBuffer Debug");
    gbuffer.Set(GBufferTextureType::GBufferDebug,
                static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(gbufferRequestDebug)));

    stltype::vector<BindlessTextureHandle> gbufferHandles = {
        g_pTexManager->MakeTextureBindless(gbufferRequestPosition.handle),
        g_pTexManager->MakeTextureBindless(gbufferRequestNormal.handle),
        g_pTexManager->MakeTextureBindless(gbufferRequestUV.handle),
        g_pTexManager->MakeTextureBindless(gbufferRequestUI.handle),
        g_pTexManager->MakeTextureBindless(gbufferRequestDebug.handle)};

    // Add depth texture handle
    gbufferHandles.push_back(m_depthBindlessHandle);

    // Update gbuffer descriptor set
    memcpy(m_mappedGBufferPostProcessUBO, &gbufferHandles[0], sizeof(BindlessTextureHandle) * gbufferHandles.size());
    for (auto& ctx : m_frameRendererContexts)
    {
        ctx.gbufferPostProcessDescriptor->WriteBufferUpdate(m_gbufferPostProcessUBO, s_globalGbufferPostProcessUBOSlot);
    }
}

void RenderPasses::PassManager::DispatchSSBOTransfer(
    void* data, DescriptorSet* pDescriptor, u32 size, StorageBuffer* pSSBO, u32 offset, u32 dstBinding)
{
    AsyncQueueHandler::SSBOTransfer transfer{.data = data,
                                             .size = size,
                                             .offset = offset,
                                             .pDescriptorSet = pDescriptor,
                                             .pStorageBuffer = pSSBO,
                                             .dstBinding = dstBinding};
    g_pQueueHandler->SubmitTransferCommandAsync(transfer);
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

    auto& lastFrameCtx = m_frameRendererContexts[lastFrame];
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
    csm.handle = shadowMapInfo.handle = g_pTexManager->GenerateHandle();
    shadowMapInfo.extents = DirectX::XMUINT3(extents.x, extents.y, cascades);
    shadowMapInfo.format = csm.format;
    shadowMapInfo.usage = Usage::ShadowMap;
    shadowMapInfo.samplerInfo.wrapU = TextureWrapMode::CLAMP_TO_BORDER;
    shadowMapInfo.samplerInfo.wrapV = TextureWrapMode::CLAMP_TO_BORDER;
    shadowMapInfo.samplerInfo.wrapW = TextureWrapMode::CLAMP_TO_BORDER;

    csm.pTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(shadowMapInfo));
    csm.bindlessHandle = g_pTexManager->MakeTextureBindless(shadowMapInfo.handle);

    // Create per-cascade 2D image views for ImGui debug display
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
    std::memcpy(m_mappedShadowMapUBO, &csm.bindlessHandle, sizeof(BindlessTextureHandle));
    for (auto& ctx : m_frameRendererContexts)
    {
        ctx.gbufferPostProcessDescriptor->WriteBufferUpdate(m_shadowMapUBO, s_shadowmapUBOBindingSlot);
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

void RenderPasses::PassManager::TransferPassData(const PassGeometryData& passData, u32 frameIdx)
{
    // Intentionally empty - placeholder for future transfer logic
    (void)passData;
    (void)frameIdx;
}

void RenderPasses::PassManager::UpdateMainViewUBO(const void* data, size_t size, u32 frameIdx)
{
    std::memcpy(m_mappedViewUBOBuffer, data, size);
}

void RenderPasses::PassManager::UpdateShadowViewUBO(const UBO::ShadowmapViewUBO& data, u32 frameIdx)
{
    std::memcpy(m_mappedShadowViewUBO, &data, sizeof(UBO::ShadowmapViewUBO));
}

void RenderPasses::PassManager::UpdateLightClusterSSBO(const UBO::LightClusterSSBO& data, u32 frameIdx)
{
    // Upload entire LightClusterSSBO (dirLight + numLights + pad + lights array)
    DispatchSSBOTransfer((void*)&data,
                         m_frameRendererContexts[frameIdx].tileArraySSBODescriptor,
                         UBO::LightClusterSSBOSize,
                         &m_lightClusterSSBO,
                         0,
                         s_tileArrayBindingSlot);
}

const stltype::vector<PassTimingResult>& RenderPasses::PassManager::GetPassTimingResults() const
{
    static const stltype::vector<PassTimingResult> s_emptyResults;
    return m_gpuTimingQuery.IsEnabled() ? m_gpuTimingQuery.GetResults() : s_emptyResults;
}

f32 RenderPasses::PassManager::GetTotalGPUTimeMs() const
{
    return m_gpuTimingQuery.IsEnabled() ? m_gpuTimingQuery.GetTotalGPUTimeMs() : 0.f;
}
