#include "PassManager.h"
#include "Compositing/CompositPass.h"
#include "Core/Rendering/Core/MaterialManager.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/Utils/DescriptorLayoutUtils.h"
#include "DebugShapePass.h"
#include "ImGuiPass.h"
#include "PreProcess/DepthPrePass.h"
#include "ShadowPass.h"
#include "StaticMeshPass.h"


#include <cstring>
#include <imgui/backends/imgui_impl_vulkan.h>

// Helper implementations to break up large functions
void RenderPasses::PassManager::InitResourceManagerAndCallbacks()
{
    m_resourceManager.Init();
    g_pEventSystem->AddSceneLoadedEventCallback(
        [this](const auto&) { m_resourceManager.UploadSceneGeometry(g_pMeshManager->GetMeshes()); });
    g_pEventSystem->AddShaderHotReloadEventCallback([this](const auto&) { RebuildPipelinesForAllPasses(); });

    // Create pass objects
    AddPass(PassType::PreProcess, stltype::make_unique<RenderPasses::DepthPrePass>());
    AddPass(PassType::Main, stltype::make_unique<RenderPasses::StaticMainMeshPass>());
    AddPass(PassType::Shadow, stltype::make_unique<RenderPasses::CSMPass>());
    AddPass(PassType::Debug, stltype::make_unique<RenderPasses::DebugShapePass>());
    AddPass(PassType::UI, stltype::make_unique<RenderPasses::ImGuiPass>());
    AddPass(PassType::Composite, stltype::make_unique<RenderPasses::CompositPass>());
}

void RenderPasses::PassManager::CreatePassObjectsAndLayouts()
{
    DescriptorPoolCreateInfo info{};
    info.enableBindlessTextureDescriptors = false;
    info.enableStorageBufferDescriptors = true;
    m_descriptorPool.Create(info);

    m_tileArraySSBOLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetForAll(
        {PipelineDescriptorLayout(UBO::BufferType::TileArraySSBO),
         PipelineDescriptorLayout(UBO::BufferType::LightUniformsUBO)});
    m_viewUBOLayout =
        DescriptorLaytoutUtils::CreateOneDescriptorSetForAll({PipelineDescriptorLayout(UBO::BufferType::View)});
    m_gbufferPostProcessLayout =
        DescriptorLaytoutUtils::CreateOneDescriptorSetForAll({PipelineDescriptorLayout(UBO::BufferType::GBufferUBO),
                                                              PipelineDescriptorLayout(UBO::BufferType::ShadowmapUBO)});
    m_shadowViewUBOLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetForAll(
        {PipelineDescriptorLayout(UBO::BufferType::ShadowmapViewUBO)});
}

void RenderPasses::PassManager::CreateUBOsAndMap()
{
    u64 tileArraySize = UBO::GlobalTileArraySSBOSize;
    u64 viewUBOSize = sizeof(UBO::ViewUBO);

    m_tileArraySSBO = StorageBuffer(tileArraySize, true);

    m_viewUBO = UniformBuffer(viewUBOSize);
    m_mappedViewUBOBuffer = m_viewUBO.MapMemory();
    m_lightUniformsUBO = UniformBuffer(sizeof(LightUniforms));
    m_gbufferPostProcessUBO = UniformBuffer(sizeof(UBO::GBufferPostProcessUBO));
    m_shadowMapUBO = UniformBuffer(sizeof(UBO::ShadowMapUBO));

    m_mappedLightUniformsUBO = m_lightUniformsUBO.MapMemory();
    m_mappedGBufferPostProcessUBO = m_gbufferPostProcessUBO.MapMemory();
    m_mappedShadowMapUBO = m_shadowMapUBO.MapMemory();
}

void RenderPasses::PassManager::CreateFrameRendererContexts()
{
    for (size_t i = 0; i < SWAPCHAIN_IMAGES; i++)
    {
        FrameRendererContext frameContext{};
        frameContext.pInitialLayoutTransitionSignalSemaphore.Create();
        frameContext.pPresentLayoutTransitionSignalSemaphore.Create();
        frameContext.nonLoadRenderingFinished.Create();
        frameContext.toReadTransitionFinished.Create();
        frameContext.shadowViewUBODescriptor = m_descriptorPool.CreateDescriptorSet(m_shadowViewUBOLayout.GetRef());
        frameContext.mainViewUBODescriptor = m_descriptorPool.CreateDescriptorSet(m_viewUBOLayout.GetRef());
        frameContext.mainViewUBODescriptor->SetBindingSlot(s_viewBindingSlot);

        frameContext.gbufferPostProcessDescriptor =
            m_descriptorPool.CreateDescriptorSet(m_gbufferPostProcessLayout.GetRef());
        frameContext.gbufferPostProcessDescriptor->SetBindingSlot(s_globalGbufferPostProcessUBOSlot);

        const auto numberString = stltype::to_string(i);
        frameContext.pInitialLayoutTransitionSignalSemaphore.SetName("Initial Layout Transition Signal Semaphore " +
                                                                     numberString);
        frameContext.pPresentLayoutTransitionSignalSemaphore.SetName("Present Layout Transition Signal Semaphore " +
                                                                     numberString);
        frameContext.nonLoadRenderingFinished.SetName("Non Load Rendering Finished Semaphore " + numberString);
        frameContext.toReadTransitionFinished.SetName("To Read Transition Finished Semaphore " + numberString);
        m_imageAvailableSemaphores[i].Create();
        m_imageAvailableFences[i].Create(false);
        m_imageAvailableFences[i].SetName("Image Available Fence " + numberString);
        m_imageAvailableSemaphores[i].SetName("Image Available Semaphore " + numberString);

        // Tile array data
        frameContext.tileArraySSBODescriptor = m_descriptorPool.CreateDescriptorSet(m_tileArraySSBOLayout.GetRef());
        frameContext.tileArraySSBODescriptor->SetBindingSlot(s_tileArrayBindingSlot);

        m_frameRendererContexts.push_back(frameContext);

        UBO::ViewUBO view{};
        UpdateMainViewUBO((const void*)&view, sizeof(UBO::ViewUBO), i);
    }
}

void RenderPasses::PassManager::InitPassesAndImGui()
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

    RecreateGbuffers(mathstl::Vector2(FrameGlobals::GetSwapChainExtent().x, FrameGlobals::GetSwapChainExtent().y));

    ColorAttachmentInfo colorAttachmentInfo{};
    colorAttachmentInfo.format = VkGlobals::GetSwapChainImageFormat();
    colorAttachmentInfo.finalLayout = ImageLayout::COLOR_ATTACHMENT;
    auto colorAttachment = ColorAttachment::Create(colorAttachmentInfo);

    DepthBufferAttachmentInfo depthAttachmentInfo{};
    depthAttachmentInfo.format = DEPTH_BUFFER_FORMAT;
    // create depth texture as in original implementation
    DynamicTextureRequest depthRequest{};
    depthRequest.handle = g_pTexManager->GenerateHandle();
    depthRequest.extents =
        DirectX::XMUINT3(FrameGlobals::GetSwapChainExtent().x, FrameGlobals::GetSwapChainExtent().y, 1);
    depthRequest.format = DEPTH_BUFFER_FORMAT;
    depthRequest.usage = Usage::StencilAttachment;
    auto* pDepthTex = g_pTexManager->CreateTextureImmediate(depthRequest);
    auto depthAttachment = DepthBufferAttachmentVulkan::Create(depthAttachmentInfo, pDepthTex);

    m_globalRendererAttachments.colorAttachments[ColorAttachmentType::GBufferColor].push_back(colorAttachment);
    m_globalRendererAttachments.depthAttachment = depthAttachment;

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
    VkDescriptorSet csmId =
        ImGui_ImplVulkan_AddTexture(m_globalRendererAttachments.directionalLightShadowMap.pTexture->GetSampler(),
                                    m_globalRendererAttachments.directionalLightShadowMap.pTexture->GetImageView(),
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorSet id1 = ImGui_ImplVulkan_AddTexture(m_gbuffer.Get(GBufferTextureType::GBufferNormal)->GetSampler(),
                                                      m_gbuffer.Get(GBufferTextureType::GBufferNormal)->GetImageView(),
                                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorSet id2 = ImGui_ImplVulkan_AddTexture(m_gbuffer.Get(GBufferTextureType::GBufferAlbedo)->GetSampler(),
                                                      m_gbuffer.Get(GBufferTextureType::GBufferAlbedo)->GetImageView(),
                                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorSet id3 =
        ImGui_ImplVulkan_AddTexture(m_gbuffer.Get(GBufferTextureType::TexCoordMatData)->GetSampler(),
                                    m_gbuffer.Get(GBufferTextureType::TexCoordMatData)->GetImageView(),
                                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkDescriptorSet id4 = ImGui_ImplVulkan_AddTexture(m_gbuffer.Get(GBufferTextureType::GBufferUI)->GetSampler(),
                                                      m_gbuffer.Get(GBufferTextureType::GBufferUI)->GetImageView(),
                                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    g_pApplicationState->RegisterUpdateFunction(
        [depthId, csmId, id1, id2, id3, id4](auto& state)
        {
            state.renderState.dirLightCSMImGuiID = reinterpret_cast<u64>(csmId);
            state.renderState.depthbufferImGuiID = reinterpret_cast<u64>(depthId);
            state.renderState.gbufferImGuiIDs.clear();
            state.renderState.gbufferImGuiIDs.push_back(reinterpret_cast<u64>(id1));
            state.renderState.gbufferImGuiIDs.push_back(reinterpret_cast<u64>(id2));
            state.renderState.gbufferImGuiIDs.push_back(reinterpret_cast<u64>(id3));
            state.renderState.gbufferImGuiIDs.push_back(reinterpret_cast<u64>(id4));
        });

    g_pQueueHandler->WaitForFences();
}

bool RenderPasses::PassManager::AnyPassWantsToRender() const
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

void RenderPasses::PassManager::BuildSyncContextsIfNeeded(bool& rebuildSyncs, FrameRendererContext& ctx)
{
    auto& syncContexts = ctx.synchronizationContexts;

    for (const auto& passes : m_passes)
    {
        if (passes.first == PassType::PostProcess || passes.first == PassType::Composite)
            continue;

        for (const auto& pass : passes.second)
        {
            if ((pass->WantsToRender() && syncContexts.find(pass.get()) == syncContexts.end()) ||
                (pass->WantsToRender() == false && syncContexts.find(pass.get()) != syncContexts.end()))
            {
                rebuildSyncs = true;
                break;
            }
        }
    }

    if (rebuildSyncs)
    {
        auto& additionalSyncContexts = ctx.additionalSynchronizationContexts;
        Semaphore* waitSemaphore = &ctx.pInitialLayoutTransitionSignalSemaphore;

        auto fillSyncContextForPasses = [](const auto& passes, auto& syncContexts, Semaphore*& waitSemaphore)
        {
            for (auto& pass : passes)
            {
                if (pass->WantsToRender())
                {
                    auto& syncContext = syncContexts[pass.get()];
                    syncContext.signalSemaphore.Create();
                    syncContext.waitSemaphore = waitSemaphore;
                    waitSemaphore = &syncContext.signalSemaphore;
                }
            }
        };

        auto AddNonPassDependency = [](stltype::vector<RenderPassSynchronizationContext>& additionalSyncContexts,
                                       Semaphore*& waitSemaphore,
                                       u32 stageIdx)
        {
            additionalSyncContexts.emplace(additionalSyncContexts.begin() + stageIdx, waitSemaphore);
            additionalSyncContexts[stageIdx].signalSemaphore.Create();
            waitSemaphore = &additionalSyncContexts[stageIdx].signalSemaphore;
        };

        syncContexts.clear();
        syncContexts.reserve(100);
        additionalSyncContexts.clear();
        additionalSyncContexts.reserve(100);
        fillSyncContextForPasses(m_passes[PassType::PreProcess], syncContexts, waitSemaphore);
        fillSyncContextForPasses(m_passes[PassType::Main], syncContexts, waitSemaphore);
        fillSyncContextForPasses(m_passes[PassType::Debug], syncContexts, waitSemaphore);
        fillSyncContextForPasses(m_passes[PassType::Shadow], syncContexts, waitSemaphore);

        AddNonPassDependency(additionalSyncContexts, waitSemaphore, 0);
        // AddNonPassDependency(additionalSyncContexts, waitSemaphore, shadowReadTransition);
        fillSyncContextForPasses(m_passes[PassType::UI], syncContexts, waitSemaphore);

        AddNonPassDependency(additionalSyncContexts, waitSemaphore, 1);

        fillSyncContextForPasses(m_passes[PassType::Composite], syncContexts, waitSemaphore);

        AddNonPassDependency(additionalSyncContexts, waitSemaphore, 2);
        ctx.renderingFinishedSemaphore = waitSemaphore;
    }
}

void RenderPasses::PassManager::PrepareMainPassDataForFrame(MainPassData& mainPassData,
                                                            FrameRendererContext& ctx,
                                                            u32 frameIdx)
{
    mainPassData.pResourceManager = &m_resourceManager;
    mainPassData.pGbuffer = &m_gbuffer;
    mainPassData.mainView.descriptorSet = ctx.mainViewUBODescriptor;
    mainPassData.directionalLightShadowMap = m_globalRendererAttachments.directionalLightShadowMap;
    mainPassData.cascades = m_currentShadowMapState.cascadeCount;
}

void RenderPasses::PassManager::PerformInitialLayoutTransitions(FrameRendererContext& ctx,
                                                                const stltype::vector<const Texture*>& gbufferTextures,
                                                                Texture* pSwapChainTexture,
                                                                Semaphore& imageAvailableSemaphore)
{
    // Transfer swap chain image from present to render layout before rendering
    AsyncLayoutTransitionRequest transitionRequest{.textures = gbufferTextures,
                                                   .oldLayout = ImageLayout::UNDEFINED,
                                                   .newLayout = ImageLayout::COLOR_ATTACHMENT,
                                                   .pWaitSemaphore = &imageAvailableSemaphore,
                                                   .pSignalSemaphore = &ctx.pInitialLayoutTransitionSignalSemaphore};
    g_pTexManager->EnqueueAsyncImageLayoutTransition(transitionRequest);

    {
        AsyncLayoutTransitionRequest transitionRequest{
            .textures = {m_globalRendererAttachments.depthAttachment.GetTexture(),
                         m_globalRendererAttachments.directionalLightShadowMap.pTexture},
            .oldLayout = ImageLayout::UNDEFINED,
            .newLayout = ImageLayout::DEPTH_STENCIL};
        g_pTexManager->EnqueueAsyncImageLayoutTransition(transitionRequest);
    }
    g_pTexManager->DispatchAsyncOps("Initial layout transition");
}

void RenderPasses::PassManager::RenderAllPassGroups(
    const MainPassData& mainPassData,
    FrameRendererContext& ctx,
    const stltype::hash_map<ConvolutionRenderPass*, RenderPassSynchronizationContext>& syncContexts,
    Semaphore& imageAvailableSemaphore)
{
    auto RenderAllPasses =
        [](auto& passes, const MainPassData& mainPassData, FrameRendererContext& ctx, const auto& syncContexts)
    {
        for (auto& pass : passes)
        {
            if (syncContexts.find(pass.get()) == syncContexts.end())
                continue;

            pass->Render(mainPassData, ctx);
        }
    };

    stltype::vector<const Texture*> gbufferTextures = m_gbuffer.GetAllTexturesWithoutUI();
    const auto* pUITexture = m_gbuffer.Get(GBufferTextureType::GBufferUI);
    stltype::vector<const Texture*> gbufferAndSwapchainTextures = gbufferTextures;
    gbufferAndSwapchainTextures.push_back(ctx.pCurrentSwapchainTexture);
    gbufferAndSwapchainTextures.push_back(pUITexture);
    // Transfer swap chain image from present to render layout before rendering
    auto pSwapChainTexture = ctx.pCurrentSwapchainTexture;

    PerformInitialLayoutTransitions(ctx, gbufferAndSwapchainTextures, pSwapChainTexture, imageAvailableSemaphore);

    RenderAllPasses(m_passes[PassType::PreProcess], mainPassData, ctx, syncContexts);
    RenderAllPasses(m_passes[PassType::Main], mainPassData, ctx, syncContexts);
    RenderAllPasses(m_passes[PassType::Debug], mainPassData, ctx, syncContexts);
    RenderAllPasses(m_passes[PassType::Shadow], mainPassData, ctx, syncContexts);

    {
        AsyncLayoutTransitionRequest transitionRequest{
            .textures = gbufferTextures,
            .oldLayout = ImageLayout::COLOR_ATTACHMENT,
            .newLayout = ImageLayout::SHADER_READ_OPTIMAL,
            .pWaitSemaphore = ctx.additionalSynchronizationContexts[0].waitSemaphore,
            .pSignalSemaphore = &ctx.additionalSynchronizationContexts[0].signalSemaphore};
        g_pTexManager->EnqueueAsyncImageLayoutTransition(transitionRequest);
    }
    {
        AsyncLayoutTransitionRequest transitionRequest{
            .textures = {m_globalRendererAttachments.directionalLightShadowMap.pTexture},
            .oldLayout = ImageLayout::DEPTH_STENCIL,
            .newLayout = ImageLayout::SHADER_READ_OPTIMAL,
        };
        g_pTexManager->EnqueueAsyncImageLayoutTransition(transitionRequest);
    }
    g_pTexManager->DispatchAsyncOps("Color to shader read transition");
    RenderAllPasses(m_passes[PassType::UI], mainPassData, ctx, syncContexts);
    {
        AsyncLayoutTransitionRequest transitionRequest{
            .textures = {pUITexture},
            .oldLayout = ImageLayout::COLOR_ATTACHMENT,
            .newLayout = ImageLayout::SHADER_READ_OPTIMAL,
            .pWaitSemaphore = ctx.additionalSynchronizationContexts[1].waitSemaphore,
            .pSignalSemaphore = &ctx.additionalSynchronizationContexts[1].signalSemaphore};
        g_pTexManager->EnqueueAsyncImageLayoutTransition(transitionRequest);
    }
    g_pTexManager->DispatchAsyncOps("UI gbuffer to shader read transition");
    RenderAllPasses(m_passes[PassType::Composite], mainPassData, ctx, syncContexts);

    {
        AsyncLayoutTransitionRequest transitionRequest{
            .textures = {ctx.pCurrentSwapchainTexture},
            .oldLayout = ImageLayout::COLOR_ATTACHMENT,
            .newLayout = ImageLayout::PRESENT,
            .pWaitSemaphore = ctx.additionalSynchronizationContexts[2].waitSemaphore,
            .pSignalSemaphore = &ctx.additionalSynchronizationContexts[2].signalSemaphore};
        g_pTexManager->EnqueueAsyncImageLayoutTransition(transitionRequest);
    }
    g_pTexManager->DispatchAsyncOps("Color to present transition");
}

void RenderPasses::PassManager::Init()
{
    InitResourceManagerAndCallbacks();
    CreatePassObjectsAndLayouts();
    CreateUBOsAndMap();
    CreateFrameRendererContexts();
    InitPassesAndImGui();
}

void RenderPasses::PassManager::ExecutePasses(u32 frameIdx)
{
    auto& mainPassData = m_mainPassData.at(frameIdx);

    if (!AnyPassWantsToRender())
        return;

    auto& ctx = m_frameRendererContexts.at(m_currentSwapChainIdx);

    bool rebuildSyncs = false;
    BuildSyncContextsIfNeeded(rebuildSyncs, ctx);

    auto& imageAvailableSemaphore = m_imageAvailableSemaphores.at(frameIdx);

    ctx.imageIdx = m_currentSwapChainIdx;
    ctx.currentFrame = frameIdx;
    ctx.pCurrentSwapchainTexture = &g_pTexManager->GetSwapChainTextures().at(m_currentSwapChainIdx);

    PrepareMainPassDataForFrame(mainPassData, ctx, frameIdx);

    // Use the synchronization contexts for the current frame
    auto& syncContexts = ctx.synchronizationContexts;

    RenderAllPassGroups(mainPassData, ctx, syncContexts, imageAvailableSemaphore);

    AsyncQueueHandler::PresentRequest presentRequest{.pWaitSemaphore =
                                                         &ctx.additionalSynchronizationContexts[2].signalSemaphore,
                                                     .swapChainImageIdx = m_currentSwapChainIdx};
    g_pQueueHandler->SubmitSwapchainPresentRequestForThisFrame(presentRequest);
    g_pQueueHandler->DispatchAllRequests();
}

// Ensure destructor symbol exists
RenderPasses::PassManager::~PassManager()
{
    // cleanup if necessary
}

// Restore functions removed during refactor
void RenderPasses::PassManager::AddPass(PassType type, stltype::unique_ptr<ConvolutionRenderPass>&& pass)
{
    m_passes[type].push_back(std::move(pass));
}

void RenderPasses::PassManager::SetEntityMeshDataForFrame(EntityMeshDataMap&& data, u32 frameIdx)
{
    // DEBUG_ASSERT(frameIdx == FrameGlobals::GetFrameNumber());
    m_passDataMutex.lock();
    m_dataToBePreProcessed.entityMeshData = std::move(data);
    m_dataToBePreProcessed.frameIdx = frameIdx;
    m_passDataMutex.unlock();
}

void RenderPasses::PassManager::SetEntityTransformDataForFrame(TransformSystemData&& data, u32 frameIdx)
{
    // DEBUG_ASSERT(frameIdx == FrameGlobals::GetFrameNumber());
    m_passDataMutex.lock();
    m_dataToBePreProcessed.entityTransformData = std::move(data);
    m_dataToBePreProcessed.frameIdx = frameIdx;
    m_passDataMutex.unlock();
}

void RenderPasses::PassManager::SetLightDataForFrame(PointLightVector&& data, DirLightVector&& dirLights, u32 frameIdx)
{
    // DEBUG_ASSERT(frameIdx == FrameGlobals::GetFrameNumber());
    m_passDataMutex.lock();
    m_dataToBePreProcessed.lightVector = data;
    m_dataToBePreProcessed.dirLightVector = dirLights;
    m_dataToBePreProcessed.frameIdx = frameIdx;
    m_passDataMutex.unlock();
}

void RenderPasses::PassManager::SetMainViewData(UBO::ViewUBO&& viewUBO, u32 frameIdx)
{
    m_passDataMutex.lock();
    m_dataToBePreProcessed.mainViewUBO = std::move(viewUBO);
    m_dataToBePreProcessed.frameIdx = frameIdx;
    m_passDataMutex.unlock();
}

void RenderPasses::PassManager::PreProcessDataForCurrentFrame(u32 frameIdx)
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
        const auto& ctx = m_frameRendererContexts[targetFrame];

        m_resourceManager.WriteInstanceSSBODescriptorUpdate(targetFrame);

        ctx.mainViewUBODescriptor->WriteBufferUpdate(m_viewUBO);
        ctx.tileArraySSBODescriptor->WriteSSBOUpdate(m_tileArraySSBO);
        ctx.tileArraySSBODescriptor->WriteBufferUpdate(m_lightUniformsUBO, s_globalLightUniformsBindingSlot);
    }

    if (g_pMaterialManager->IsBufferDirty())
    {
        m_resourceManager.UpdateGlobalMaterialBuffer(g_pMaterialManager->GetMaterialBuffer(), frameIdx);
        m_needsToPropagateMainDataUpdate = true;
        m_frameIdxToPropagate = frameIdx;
        g_pMaterialManager->MarkBufferUploaded();
    }

    if (m_dataToBePreProcessed.IsEmpty() == false)
    {
        DEBUG_ASSERT(m_dataToBePreProcessed.IsValid());
        PassGeometryData passData{};

        stltype::vector<DirectX::XMFLOAT4X4> transformSSBO;
        transformSSBO.reserve(MAX_ENTITIES);

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
                m_resourceManager.UpdateInstanceDataSSBO(passData.staticMeshPassData, frameIdx);
                PreProcessMeshData(
                    passData.staticMeshPassData, FrameGlobals::GetPreviousFrameNumber(frameIdx), frameIdx);
            }
            TransferPassData(std::move(passData), m_dataToBePreProcessed.frameIdx);
        }

        for (const auto& data : m_dataToBePreProcessed.entityTransformData)
        {
            const auto& entityID = data.first;
            transformSSBO.insert(transformSSBO.begin() + m_entityToTransformUBOIdx[entityID], data.second);
        }
        m_resourceManager.UpdateTransformBuffer(transformSSBO, m_dataToBePreProcessed.frameIdx);

        if (m_dataToBePreProcessed.mainViewUBO.has_value())
        {
            UpdateMainViewUBO(
                &m_dataToBePreProcessed.mainViewUBO.value(), sizeof(UBO::ViewUBO), m_dataToBePreProcessed.frameIdx);
        }

        auto& mainPassData = m_mainPassData.at(frameIdx);
        // Light uniforms
        {
            LightUniforms data;
            const auto camEnt = g_pApplicationState->GetCurrentApplicationState().mainCameraEntity;
            const ECS::Components::Camera* camComp = g_pEntityManager->GetComponent<ECS::Components::Camera>(camEnt);
            mathstl::Matrix camMatrix = *(transformSSBO.begin() + m_entityToTransformUBOIdx[camEnt.ID]);
            mathstl::Vector3 camPos, camScale;
            mathstl::Quaternion camRot;
            camMatrix.Decompose(camScale, camRot, camPos);

            data.CameraPos = mathstl::Vector4(camPos.x, camPos.y, camPos.z, 1);
            data.LightGlobals = mathstl::Vector4(0.6, 1, 1, 1);

            mainPassData.mainView.viewport = RenderViewUtils::CreateViewportFromData(
                FrameGlobals::GetSwapChainExtent(), camComp->zNear, camComp->zFar);
            mainPassData.mainView.fov = camComp->fov;
            auto pDescriptor = m_frameRendererContexts[m_dataToBePreProcessed.frameIdx].tileArraySSBODescriptor;
            auto* pBuffer = &m_lightUniformsUBO;
            auto mappedBuffer = m_mappedLightUniformsUBO;

            memcpy(mappedBuffer, &data, pBuffer->GetInfo().size);
            pDescriptor->WriteBufferUpdate(m_lightUniformsUBO, s_globalLightUniformsBindingSlot);
        }

        if (m_dataToBePreProcessed.lightVector.empty() == false ||
            m_dataToBePreProcessed.dirLightVector.empty() == false)
        {
            // Light data
            // Also cull lights and so on here
            auto& tileArray = m_tileArray;

            tileArray.tiles.clear();
            tileArray.tiles.resize(MAX_TILES);
            tileArray.tiles[0].lights.reserve(MAX_LIGHTS_PER_TILE);
            for (const auto& light : m_dataToBePreProcessed.lightVector)
            {
                tileArray.tiles[0].lights.push_back(light);
            }
            if (m_dataToBePreProcessed.dirLightVector.empty() == false)
            {
                const auto& dirLight = m_dataToBePreProcessed.dirLightVector[0];
                tileArray.dirLight.direction =
                    mathstl::Vector4(dirLight.direction.x, dirLight.direction.y, dirLight.direction.z, 1);
                tileArray.dirLight.direction.Normalize();
                tileArray.dirLight.color = mathstl::Vector4(dirLight.color.x, dirLight.color.y, dirLight.color.z, 1);
            }
            UpdateWholeTileArraySSBO(tileArray, frameIdx);

            // Directional lights
            {
                const auto& dirLights = m_dataToBePreProcessed.dirLightVector;
                mainPassData.csmViews.clear();
                for (const auto& dirLight : dirLights)
                {
                    auto& dirLightView = mainPassData.csmViews.emplace_back();
                    dirLightView.dir =
                        mathstl::Vector3(dirLight.direction.x, dirLight.direction.y, dirLight.direction.z);
                    dirLightView.cascades = m_currentShadowMapState.cascadeCount;
                }
            }
        }

        // Clear
        m_needsToPropagateMainDataUpdate = true;
        m_frameIdxToPropagate = frameIdx;
        m_dataToBePreProcessed.Clear();
        g_pQueueHandler->DispatchAllRequests();
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
    gbuffer.Set(GBufferTextureType::GBufferAlbedo, g_pTexManager->CreateTextureImmediate(gbufferRequestPosition));

    DynamicTextureRequest gbufferRequestNormal = gbufferRequestBase;
    gbufferRequestNormal.format = gbuffer.GetFormat(GBufferTextureType::GBufferNormal);
    gbufferRequestNormal.handle = g_pTexManager->GenerateHandle();
    gbufferRequestNormal.AddName("GBuffer Normal");
    gbuffer.Set(GBufferTextureType::GBufferNormal, g_pTexManager->CreateTextureImmediate(gbufferRequestNormal));

    DynamicTextureRequest gbufferRequestUV = gbufferRequestBase;
    gbufferRequestUV.format = gbuffer.GetFormat(GBufferTextureType::TexCoordMatData);
    gbufferRequestUV.handle = g_pTexManager->GenerateHandle();
    gbufferRequestUV.AddName("GBuffer UV Material Data");
    gbuffer.Set(GBufferTextureType::TexCoordMatData, g_pTexManager->CreateTextureImmediate(gbufferRequestUV));
    g_pTexManager->MakeTextureBindless(gbufferRequestUV.handle);

    DynamicTextureRequest gbufferRequest3 = gbufferRequestBase;
    gbufferRequest3.format = gbuffer.GetFormat(GBufferTextureType::Position);
    gbufferRequest3.handle = g_pTexManager->GenerateHandle();
    gbufferRequest3.AddName("GBuffer Position");
    gbuffer.Set(GBufferTextureType::Position, g_pTexManager->CreateTextureImmediate(gbufferRequest3));
    g_pTexManager->MakeTextureBindless(gbufferRequest3.handle);

    DynamicTextureRequest gbufferRequestUI = gbufferRequestBase;
    gbufferRequestUI.format = gbuffer.GetFormat(GBufferTextureType::GBufferUI);
    gbufferRequestUI.handle = g_pTexManager->GenerateHandle();
    gbufferRequestUI.AddName("GBuffer UI");
    gbuffer.Set(GBufferTextureType::GBufferUI, g_pTexManager->CreateTextureImmediate(gbufferRequestUI));

    stltype::vector<BindlessTextureHandle> gbufferHandles = {
        g_pTexManager->MakeTextureBindless(gbufferRequestPosition.handle),
        g_pTexManager->MakeTextureBindless(gbufferRequestNormal.handle),
        g_pTexManager->MakeTextureBindless(gbufferRequestUV.handle),
        g_pTexManager->MakeTextureBindless(gbufferRequest3.handle), // TODO: Replace with actual gbuffer 4 when needed
        g_pTexManager->MakeTextureBindless(gbufferRequestUI.handle)};

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
    // vkQueueWaitIdle(VkGlobals::GetGraphicsQueue());
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
    if (csm.pTexture != nullptr)
    {
        g_pTexManager->FreeTexture(csm.handle);
    }
    csm.cascades = cascades;
    csm.format = DEPTH_BUFFER_FORMAT;
    DynamicTextureRequest shadowMapInfo{};
    shadowMapInfo.AddName("Directional Light CSM");
    csm.handle = shadowMapInfo.handle = g_pTexManager->GenerateHandle();
    shadowMapInfo.extents = DirectX::XMUINT3(extents.x, extents.y, cascades);
    shadowMapInfo.format = DEPTH_BUFFER_FORMAT;
    shadowMapInfo.usage = Usage::ShadowMap;
    shadowMapInfo.samplerInfo.wrapU = TextureWrapMode::CLAMP_TO_EDGE;
    shadowMapInfo.samplerInfo.wrapV = TextureWrapMode::CLAMP_TO_EDGE;
    shadowMapInfo.samplerInfo.wrapW = TextureWrapMode::CLAMP_TO_EDGE;

    csm.pTexture = g_pTexManager->CreateTextureImmediate(shadowMapInfo);
    csm.bindlessHandle = g_pTexManager->MakeTextureBindless(shadowMapInfo.handle);
    // Update gbuffer descriptor set
    std::memcpy(m_mappedShadowMapUBO, &csm.bindlessHandle, sizeof(BindlessTextureHandle));
    for (auto& ctx : m_frameRendererContexts)
    {
        ctx.gbufferPostProcessDescriptor->WriteBufferUpdate(m_shadowMapUBO, s_shadowmapUBOBindingSlot);
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
    auto pDescriptor = m_frameRendererContexts[frameIdx].mainViewUBODescriptor;
    auto* pBuffer = &m_viewUBO;
    auto mappedBuffer = m_mappedViewUBOBuffer;

    std::memcpy(mappedBuffer, data, size);
    pDescriptor->WriteBufferUpdate(*pBuffer);
}

void RenderPasses::PassManager::UpdateWholeTileArraySSBO(const UBO::GlobalTileArraySSBO& data, u32 frameIdx)
{
    DispatchSSBOTransfer((void*)&data.dirLight,
                         m_frameRendererContexts[frameIdx].tileArraySSBODescriptor,
                         sizeof(data.dirLight),
                         &m_tileArraySSBO,
                         0,
                         s_tileArrayBindingSlot);
    DispatchSSBOTransfer((void*)data.tiles[0].lights.data(),
                         m_frameRendererContexts[frameIdx].tileArraySSBODescriptor,
                         UBO::GlobalTileArraySSBOSize - sizeof(data.dirLight),
                         &m_tileArraySSBO,
                         sizeof(data.dirLight),
                         s_tileArrayBindingSlot);
}
