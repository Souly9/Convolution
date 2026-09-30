#include "FrameResourceManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/Rendering/Core/MaterialManager.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Core/AntiAliasing.h"
#include "Core/Rendering/Core/Utils/TAA/JitterFunctions.h"
#include "Core/Rendering/Core/View.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Core/Rendering/Passes/ShadowPass.h"
#include "Core/Rendering/Core/DescriptorUtils/DescriptorLayoutUtils.h"
#include "Core/Rendering/Core/DescriptorPool.h"
#include "Core/Rendering/Core/Texture.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/WindowManager.h"
#include <EASTL/algorithm.h>

namespace RenderPasses
{
void FrameResourceManager::BuildSharedDataForView(const RenderView& mainView,
                                                  const mathstl::Vector2& renderResolution,
                                                  const mathstl::Vector2& outputResolution,
                                                  u64 jitterFrameNumber,
                                                  UBO::SharedDataUBO& ubo) const
{
    ScopedZone("BuildSharedDataForView");
    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    const AA::FrameConfig& aa = AA::Current();
    using namespace mathstl;

    const f32 fovRadians = DirectX::XMConvertToRadians(mainView.fov);
    const Vector3 rotation = Vector3(DirectX::XMConvertToRadians(mainView.rotation.x),
                                     DirectX::XMConvertToRadians(mainView.rotation.y),
                                     DirectX::XMConvertToRadians(mainView.rotation.z));
    const Vector3& viewPos = mainView.position;

    Matrix rotationMatrix = Matrix::CreateFromYawPitchRoll(rotation.y, rotation.x, rotation.z);
    Vector3 forward = Vector3::Transform(Vector3(0, 0, 1), rotationMatrix);
    Vector3 rotatedFocusPos = viewPos - forward;
    const Vector3 upVector(0.f, 1.f, 0.f);
    const f32 aspectRatio = (renderResolution.y > 0.0f) ? (renderResolution.x / renderResolution.y) : 1.0f;

    const Matrix viewMat = Matrix::CreateLookAt(viewPos, rotatedFocusPos, upVector);
    const f32 nearPlane = (mainView.zNear > 0.0f) ? mainView.zNear : 0.1f;
    const f32 farPlane = (mainView.zFar > nearPlane) ? mainView.zFar : 1000.0f;
    // Reversed Z: near and far are swapped on purpose
    const Matrix projMat = Matrix::CreatePerspectiveFieldOfView(fovRadians, aspectRatio, farPlane, nearPlane);
    const Matrix viewProj = viewMat * projMat;

    ubo.prevViewProjection = aa.temporalReset ? viewProj : ubo.viewProjection;
    ubo.view = viewMat;
    ubo.projection = projMat;
    ubo.viewProjection = viewProj;
    ubo.viewProjectionInverse = viewProj.Invert();
    ubo.viewInverse = viewMat.Invert();
    ubo.projectionInverse = projMat.Invert();
    ubo.clipToPrevClip = ubo.projectionInverse * ubo.viewInverse * ubo.prevViewProjection;
    ubo.viewPos = Vector4(viewPos.x, viewPos.y, viewPos.z, 1.0f);
    ubo.renderResolution = renderResolution;
    ubo.outputResolution = outputResolution;
    ubo.jitterOffset = aa.UsesJitter() ? GenerateHaltonJitter(jitterFrameNumber, aa.jitterPhases) : Vector2::Zero;
    ubo.zNear = nearPlane;
    ubo.zFar = farPlane;
    ubo.fovY = fovRadians;
    ubo.aspectRatio = aspectRatio;
    ubo.temporalReset = aa.temporalReset ? 1u : 0u;

    // Debug state flags
    ubo.debugFlags = renderState.debugFlags;
    ubo.debugViewMode = renderState.debugViewMode;
    ubo.exposure = renderState.exposure;
    ubo.toneMapperType = renderState.toneMapperType;
    ubo.ambientIntensity = renderState.ambientIntensity;
    ubo.gt7PaperWhite = renderState.gt7PaperWhite;
    ubo.gt7ReferenceLuminance = renderState.gt7ReferenceLuminance;
    ubo.rtUseGlobalMaterialReflectance = renderState.rt.globalReflectanceOverrideEnabled ? 1u : 0u;
    ubo.rtGlobalMaterialReflectance = renderState.rt.globalMaterialReflectance;
    ubo.skyboxTextureIdx = m_skyboxBindlessHandle;
}

void FrameResourceManager::Init()
{
    u64 sharedDataUBOSize = sizeof(UBO::SharedDataUBO);

    m_lightCluster = stltype::make_unique<UBO::LightClusterSSBO>();
    m_lightCluster->lights.resize(MAX_SCENE_LIGHTS);
    m_lightClusterSSBO = StorageBuffer(UBO::LightClusterSSBOSize, false);
    m_clusterGridSSBO = StorageBuffer(UBO::ClusterAABBSetSize, true);

    m_sharedDataUBO.Create(sharedDataUBOSize, "Shared Data UBO");
    m_lightUniformsUBO.Create(sizeof(LightUniforms), "Light Uniforms UBO");
    m_gbufferPostProcessUBO.Create(sizeof(UBO::GBufferPostProcessUBO), "GBuffer PostProcess UBO");
    m_shadowMapUBO.Create(sizeof(UBO::ShadowMapUBO), "Shadow Map UBO");

    m_lightClusterSSBO.SetName("Light Cluster SSBO");
    m_clusterGridSSBO.SetName("Cluster AABB SSBO");

    m_cachedTransformSSBO.resize(MAX_ENTITIES);
    m_cachedPrevTransformSSBO.resize(MAX_ENTITIES);
    m_cachedSceneAABBs.resize(MAX_ENTITIES);

    m_skyboxTextureHandle = g_pTexManager->SubmitAsyncTextureCreation(
        {"../../Resources/Skyboxes/mpumalanga_veld_puresky_4k.hdr", false, TextureSemantic::Auto, true});
    m_skyboxBindlessHandle = g_pTexManager->MakeTextureBindless(m_skyboxTextureHandle, true);

}

void FrameResourceManager::CreatePassObjectsAndLayouts()
{
    DescriptorPoolCreateInfo info{};
    info.enableBindlessTextureDescriptors = false;
    info.enableStorageBufferDescriptors = true;
    m_descriptorPool.Create(
        {.enableBindlessTextureDescriptors = true, .enableStorageBufferDescriptors = true, .freeDescriptorSet = true});
    m_descriptorPool.SetName("Global Frame Descriptor Pool");

    m_lightClusterSSBOLayout = DescriptorLayoutUtils::CreateOneDescriptorSetForAll(
        {PipelineDescriptorLayout(UBO::BufferType::TileArraySSBO),
         PipelineDescriptorLayout(UBO::BufferType::LightUniformsUBO)});
    m_sharedDataUBOLayout =
        DescriptorLayoutUtils::CreateOneDescriptorSetForAll({PipelineDescriptorLayout(UBO::BufferType::View)});
    m_gbufferPostProcessLayout =
        DescriptorLayoutUtils::CreateOneDescriptorSetForAll({PipelineDescriptorLayout(UBO::BufferType::GBufferUBO),
                                                             PipelineDescriptorLayout(UBO::BufferType::ShadowmapUBO)});

    // Cluster grid
    m_clusterGridSSBOLayout = DescriptorLayoutUtils::CreateOneDescriptorSetForAll(
        {PipelineDescriptorLayout(UBO::BufferType::ClusterAABBsSSBO)});

    m_lightClusterSSBOLayout.SetName("Light Cluster Layout");
    m_sharedDataUBOLayout.SetName("Shared Data Layout");
    m_gbufferPostProcessLayout.SetName("GBuffer PostProcess Layout");
    m_clusterGridSSBOLayout.SetName("Cluster Grid Layout");
}

void FrameResourceManager::CreateFrameRendererContexts(
    stltype::fixed_vector<Semaphore, SWAPCHAIN_IMAGES>& imageAvailableSemaphores,
    stltype::fixed_vector<Fence, SWAPCHAIN_IMAGES>& imageAvailableFences,
    stltype::fixed_vector<Fence, SWAPCHAIN_IMAGES>& renderFinishedFences)
{
    m_frameRendererContexts.resize(SWAPCHAIN_IMAGES);
    for (size_t i = 0; i < SWAPCHAIN_IMAGES; i++)
    {
        auto& frameContext = m_frameRendererContexts[i];
        frameContext.frameTimeline.Create(0);
        frameContext.computeTimeline.Create(0);
        frameContext.nextTimelineValue = 0;
        frameContext.nextComputeTimelineValue = 0;
        frameContext.pPresentLayoutTransitionSignalSemaphore.Create();

        frameContext.sharedDataUBODescriptor = m_descriptorPool.CreateDescriptorSet(m_sharedDataUBOLayout.GetRef());
        frameContext.sharedDataUBODescriptor->SetBindingSlot(s_sharedDataBindingSlot);
        frameContext.sharedDataUBODescriptor->WriteBufferUpdate(m_sharedDataUBO.GetBuffer(i % FRAMES_IN_FLIGHT),
                                                                s_sharedDataBindingSlot);
        frameContext.sharedDataUBODescriptor->SetName("Shared Data Descriptor Set " + stltype::to_string(i));

        frameContext.gbufferPostProcessDescriptor =
            m_descriptorPool.CreateDescriptorSet(m_gbufferPostProcessLayout.GetRef());
        frameContext.gbufferPostProcessDescriptor->WriteBufferUpdate(
            m_gbufferPostProcessUBO.GetBuffer(i % FRAMES_IN_FLIGHT), s_globalGbufferPostProcessUBOSlot);
        frameContext.gbufferPostProcessDescriptor->WriteBufferUpdate(m_shadowMapUBO.GetBuffer(i % FRAMES_IN_FLIGHT),
                                                                     s_shadowmapUBOBindingSlot);
        frameContext.gbufferPostProcessDescriptor->SetName("GBuffer PostProcess Descriptor Set " +
                                                           stltype::to_string(i));

        const auto numberString = stltype::to_string(i);
        frameContext.frameTimeline.SetName("Frame Timeline Semaphore " + numberString);
        frameContext.computeTimeline.SetName("Compute Timeline Semaphore " + numberString);
        frameContext.pPresentLayoutTransitionSignalSemaphore.SetName("Present Layout Transition Signal Semaphore " +
                                                                     numberString);
        imageAvailableSemaphores[i].Create();
        imageAvailableFences[i].Create(false);
        imageAvailableFences[i].SetName("Image Available Fence " + numberString);
        imageAvailableSemaphores[i].SetName("Image Available Semaphore " + numberString);
        // Create render finished fence as signaled so first frame doesn't wait
        renderFinishedFences[i].Create(true);
        renderFinishedFences[i].SetName("Render Finished Fence " + numberString);

        // Tile array data
        frameContext.tileArraySSBODescriptor = m_descriptorPool.CreateDescriptorSet(m_lightClusterSSBOLayout.GetRef());
        frameContext.tileArraySSBODescriptor->SetBindingSlot(s_tileArrayBindingSlot);
        frameContext.tileArraySSBODescriptor->WriteSSBOUpdate(m_lightClusterSSBO);
        frameContext.tileArraySSBODescriptor->WriteBufferUpdate(m_lightUniformsUBO.GetBuffer(i % FRAMES_IN_FLIGHT),
                                                                s_globalLightUniformsBindingSlot);

        // Cluster grid descriptor
        frameContext.clusterGridDescriptor = m_descriptorPool.CreateDescriptorSet(m_clusterGridSSBOLayout.GetRef());
        frameContext.clusterGridDescriptor->SetBindingSlot(s_clusterGridSSBOBindingSlot);
        frameContext.clusterGridDescriptor->WriteSSBOUpdate(m_clusterGridSSBO);

        // Point the rendering finished semaphore to the present transition semaphore for presentation sync
        frameContext.renderingFinishedSemaphore = &frameContext.pPresentLayoutTransitionSignalSemaphore;
    }
}

void FrameResourceManager::PreProcessDataForCurrentFrame(u32 frameIdx,
                                                         u64 jitterFrameNumber,
                                                         u32 currentSwapChainIdx,
                                                         PassManager* pPassManager)
{
    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    const auto& passManagerRenderState = pPassManager->GetRenderState();
    // Recreate shadow maps
    {
        const auto csmCascades = renderState.directionalLightCascades;
        const auto csmResolution = renderState.csmResolution;
        if (m_currentShadowMapState.cascadeCount != csmCascades ||
            m_currentShadowMapState.shadowMapExtents.x != csmResolution.x ||
            m_currentShadowMapState.shadowMapExtents.y != csmResolution.y)
        {
            pPassManager->RecreateShadowMapsPublic(csmCascades, csmResolution);
            m_currentShadowMapState.cascadeCount = csmCascades;
            m_currentShadowMapState.shadowMapExtents = csmResolution;
        }
    }
    if (m_needsToPropagateMainDataUpdate && m_dataToBePreProcessed.IsEmpty())
    {
        m_needsToPropagateMainDataUpdate = false;
        // Use currentSwapChainIdx because that's the context we are preparing for
        auto& ctx = m_frameRendererContexts[currentSwapChainIdx];

        ctx.zNear = m_dataToBePreProcessed.mainView.zNear;
        ctx.zFar = m_dataToBePreProcessed.mainView.zFar;

        ctx.tileArraySSBODescriptor->WriteSSBOUpdate(m_lightClusterSSBO);
    }

    if (g_pMaterialManager->IsBufferDirty())
    {
        g_pMaterialManager->RebuildBufferData();
        pPassManager->GetResourceManager().UpdateGlobalMaterialBuffer(g_pMaterialManager->GetMaterialBuffer(),
                                                                      currentSwapChainIdx);
        m_needsToPropagateMainDataUpdate = true;
        m_frameIdxToPropagate = frameIdx;
        g_pMaterialManager->MarkBufferUploaded();
    }

    mathstl::Matrix frameViewProj{};

    if (m_dataToBePreProcessed.mainView.zFar > 0.0f)
    {
        m_cachedMainView = m_dataToBePreProcessed.mainView;
    }
    {
        const auto& mainView = (m_dataToBePreProcessed.mainView.zFar > 0.0f) ? m_dataToBePreProcessed.mainView : m_cachedMainView;
        auto& ctx = m_frameRendererContexts[currentSwapChainIdx];
        BuildSharedDataForView(mainView,
                               passManagerRenderState.renderResolution,
                               passManagerRenderState.swapchainResolution,
                               jitterFrameNumber,
                               m_currentSharedDataUBO);
        const mathstl::Matrix& viewMat = m_currentSharedDataUBO.view;
        const mathstl::Matrix& viewProj = m_currentSharedDataUBO.viewProjection;
        frameViewProj = viewProj;

        if (m_cachedDirLights.empty() == false)
        {
            const auto& dirLight = m_cachedDirLights[0];
            mathstl::Vector3 lightDir(dirLight.direction.x, dirLight.direction.y, dirLight.direction.z);
            lightDir.Normalize();

            stltype::array<f32, 16> splits{};
            f32 zNear = stltype::max(mainView.zNear, 0.000001f);
            f32 zFar = mainView.zFar;
            f32 aspectRatio =
                passManagerRenderState.renderResolution.y > 0.0f
                    ? passManagerRenderState.renderResolution.x / passManagerRenderState.renderResolution.y
                    : 1.0f;

            CSMPass::ComputeLightViewProjMatrices(renderState.directionalLightCascades,
                                                  zNear,
                                                  zFar,
                                                  renderState.csmLambda,
                                                  mainView.fov,
                                                  aspectRatio,
                                                  viewMat,
                                                  m_currentSharedDataUBO.viewProjectionInverse,
                                                  lightDir,
                                                  splits,
                                                  m_currentSharedDataUBO.csmViewMatrices,
                                                  (u32)renderState.csmResolution.x);

            m_currentSharedDataUBO.cascadeCount = (s32)renderState.directionalLightCascades;
            m_currentSharedDataUBO.screenSpaceShadows =
                pPassManager->GetRenderGraph().GetRegistry().ResolveBindlessByID(RGResourceID::ScreenSpaceShadows);
            for (u32 i = 0; i < 4; ++i)
            {
                m_currentSharedDataUBO.cascadeSplits[i] =
                    mathstl::Vector4(splits[i * 4 + 0], splits[i * 4 + 1], splits[i * 4 + 2], splits[i * 4 + 3]);
            }
        }

        m_sharedDataUBO.Write(frameIdx, m_currentSharedDataUBO);

        UBO::ShadowMapUBO shadowMapUBO{};
        shadowMapUBO.directionalShadowMapIdx = pPassManager->GetRenderGraph().GetRegistry().GetShadowMap().bindlessHandle;
        m_shadowMapUBO.Write(frameIdx, shadowMapUBO);

        auto& passData = pPassManager->GetMainPassData(currentSwapChainIdx);
        passData.pViewData = &m_currentSharedDataUBO;

        g_pApplicationState->RegisterUpdateFunction(
            [viewInv = m_currentSharedDataUBO.viewInverse,
             projInv = m_currentSharedDataUBO.projectionInverse,
             viewProj,
             viewMat = m_currentSharedDataUBO.view,
             projMat = m_currentSharedDataUBO.projection](ApplicationState& state)
            {
                state.renderState.invMainCamProjectionMatrix = projInv;
                state.renderState.invMainCamViewMatrix = viewInv;
                state.renderState.mainCamViewProjectionMatrix = viewProj;
                state.renderState.mainCamViewMatrix = viewMat;
                state.renderState.mainCamProjectionMatrix = projMat;
            });

        passData.mainView = mainView;
        passData.mainView.viewport = RenderViewUtils::CreateViewportFromData(
            passManagerRenderState.renderResolution, mainView.zNear, mainView.zFar);

        ctx.zNear = mainView.zNear;
        ctx.zFar = mainView.zFar;

        // Contexts follow the swapchain image, the UBO copies follow the frame slot
        ctx.sharedDataUBODescriptor->WriteBufferUpdate(m_sharedDataUBO.GetBuffer(frameIdx), s_sharedDataBindingSlot);
        ctx.gbufferPostProcessDescriptor->WriteBufferUpdate(m_gbufferPostProcessUBO.GetBuffer(frameIdx),
                                                            s_globalGbufferPostProcessUBOSlot);
        ctx.gbufferPostProcessDescriptor->WriteBufferUpdate(m_shadowMapUBO.GetBuffer(frameIdx), s_shadowmapUBOBindingSlot);
    }
    {
        {
            auto& mainPassData = pPassManager->GetMainPassData(currentSwapChainIdx);
            mainPassData.csmViews.clear();
            for (const auto& dirLight : m_cachedDirLights)
            {
                auto& dirLightView = mainPassData.csmViews.emplace_back();
                dirLightView.dir = mathstl::Vector3(dirLight.direction.x, dirLight.direction.y, dirLight.direction.z);
                dirLightView.cascades = m_currentShadowMapState.cascadeCount;
            }
        }

        // Light uniforms
        {
            const auto& mainView = m_dataToBePreProcessed.mainView;
            LightUniforms data;

            float zNear = stltype::max(mainView.zNear, 0.000001f);
            float zFar = mainView.zFar;
            if (zFar <= zNear || zFar > 10000.0f)
            {
                zFar = 10000.0f;
            }
            
            u32 sliceCount = renderState.clusterCount.z;

            float logRatio = std::log(zFar / zNear);
            float scale = (float)sliceCount / logRatio;
            float bias = -std::log(zNear) * scale;

            bool shadowsEnabled = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::ShadowsEnabled);
            data.ClusterValues = mathstl::Vector4(scale, bias, (float)sliceCount, shadowsEnabled ? 1.0f : 0.0f);
            data.ClusterSize = mathstl::Vector4((float)renderState.clusterCount.x,
                                                (float)renderState.clusterCount.y,
                                                (float)renderState.clusterCount.z,
                                                0.0f);

            m_lightUniformsUBO.Write(frameIdx, data);
            m_frameRendererContexts[currentSwapChainIdx].tileArraySSBODescriptor->WriteBufferUpdate(
                m_lightUniformsUBO.GetBuffer(frameIdx), s_globalLightUniformsBindingSlot);
        }
    }

    auto& resourceManager = pPassManager->GetResourceManager();

    if (m_dataToBePreProcessed.IsEmpty() && !m_transformsPendingPrevCatchup.empty())
    {
        u32 prevDirtyMin = ~0u;
        u32 prevDirtyMax = 0;
        for (u32 ssboIdx : m_transformsPendingPrevCatchup)
        {
            m_cachedPrevTransformSSBO[ssboIdx] = m_cachedTransformSSBO[ssboIdx];
            prevDirtyMin = (stltype::min)(prevDirtyMin, ssboIdx);
            prevDirtyMax = (stltype::max)(prevDirtyMax, ssboIdx);
        }

        const u32 prevDirtyCount = prevDirtyMax - prevDirtyMin + 1;
        resourceManager.UpdatePrevTransformRange(
            m_cachedPrevTransformSSBO, prevDirtyMin, prevDirtyCount, currentSwapChainIdx);
        m_transformsPendingPrevCatchup.clear();
    }

    if (m_dataToBePreProcessed.IsEmpty() == false)
    {
        DEBUG_ASSERT(m_dataToBePreProcessed.IsValid());
        PassGeometryData passData{};

        if (m_dataToBePreProcessed.entityMeshData.empty() == false)
        {
            stltype::hash_map<u64, u32> entityToMeshIdx;
            entityToMeshIdx.reserve(m_dataToBePreProcessed.entityMeshData.size());
            u32 meshIdx = 0;

            for (const auto& data : m_dataToBePreProcessed.entityMeshData)
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
            if (needsRebuild)
            {
                pPassManager->GetResourceManager().UpdateInstanceDataSSBO(passData.staticMeshPassData,
                                                                          currentSwapChainIdx);
                const u32 previousImageIdx =
                    (currentSwapChainIdx == 0) ? (SWAPCHAIN_IMAGES - 1) : (currentSwapChainIdx - 1);
                pPassManager->PreProcessMeshDataPublic(
                    passData.staticMeshPassData, previousImageIdx, currentSwapChainIdx);
                m_currentPassGeometryState = passData;
            }
            pPassManager->TransferPassDataPublic(std::move(passData), m_dataToBePreProcessed.frameIdx);
        }

        u32 dirtyMin = ~0u;
        u32 dirtyMax = 0;
        u32 prevDirtyMin = ~0u;
        u32 prevDirtyMax = 0;

        for (u32 ssboIdx : m_transformsPendingPrevCatchup)
        {
            m_cachedPrevTransformSSBO[ssboIdx] = m_cachedTransformSSBO[ssboIdx];
            m_transformsToPropagateToPrev.push_back(ssboIdx);
        }
        m_transformsPendingPrevCatchup.clear();

        for (const auto& data : m_dataToBePreProcessed.entityTransformData)
        {
            const auto& entityID = data.first;
            auto uboIt = m_entityToTransformUBOIdx.find(entityID);
            if (uboIt == m_entityToTransformUBOIdx.end())
                continue;

            const u32 ssboIdx = uboIt->second;
            m_cachedPrevTransformSSBO[ssboIdx] = m_cachedTransformSSBO[ssboIdx];
            m_cachedTransformSSBO[ssboIdx] = data.second;
            m_transformsToPropagateToPrev.push_back(ssboIdx);
            m_transformsPendingPrevCatchup.push_back(ssboIdx);

            AABB aabb{};
            if (m_dataToBePreProcessed.entityMeshData.find(entityID) != m_dataToBePreProcessed.entityMeshData.end())
            {
                const auto& meshDataVec = m_dataToBePreProcessed.entityMeshData.at(entityID);
                if (!meshDataVec.empty())
                    aabb = meshDataVec[0].aabb;
            }
            m_cachedSceneAABBs[ssboIdx] = aabb;

            dirtyMin = (stltype::min)(dirtyMin, ssboIdx);
            dirtyMax = (stltype::max)(dirtyMax, ssboIdx);
        }

        for (u32 ssboIdx : m_transformsToPropagateToPrev)
        {
            prevDirtyMin = (stltype::min)(prevDirtyMin, ssboIdx);
            prevDirtyMax = (stltype::max)(prevDirtyMax, ssboIdx);
        }
        if (prevDirtyMin <= prevDirtyMax)
        {
            const u32 prevDirtyCount = prevDirtyMax - prevDirtyMin + 1;
            resourceManager.UpdatePrevTransformRange(
                m_cachedPrevTransformSSBO, prevDirtyMin, prevDirtyCount, currentSwapChainIdx);
        }
        m_transformsToPropagateToPrev.clear();

        if (dirtyMin <= dirtyMax)
        {
            const u32 dirtyCount = dirtyMax - dirtyMin + 1;
            resourceManager.UpdateTransformRange(m_cachedTransformSSBO, dirtyMin, dirtyCount, currentSwapChainIdx);
            resourceManager.UpdateSceneAABBRange(m_cachedSceneAABBs, dirtyMin, dirtyCount, currentSwapChainIdx);
        }

        if (m_dataToBePreProcessed.lightVector.empty() == false ||
            m_dataToBePreProcessed.dirLightVector.empty() == false)
        {
            auto& lightClusterHost = *m_lightCluster;
            lightClusterHost.numLights = 0;

            for (const auto& light : m_dataToBePreProcessed.lightVector)
            {
                if (lightClusterHost.numLights < MAX_SCENE_LIGHTS)
                {
                    lightClusterHost.lights[lightClusterHost.numLights++] = light;
                }
            }
            if (m_dataToBePreProcessed.dirLightVector.empty() == false)
            {
                m_cachedDirLights = m_dataToBePreProcessed.dirLightVector;
                const auto& dirLight = m_dataToBePreProcessed.dirLightVector[0];
                lightClusterHost.dirLight.direction =
                    mathstl::Vector4(dirLight.direction.x, dirLight.direction.y, dirLight.direction.z, 1);
                lightClusterHost.dirLight.direction.Normalize();
                lightClusterHost.dirLight.color =
                    mathstl::Vector4(dirLight.color.x, dirLight.color.y, dirLight.color.z, dirLight.color.w);
            }
            UpdateLightClusterSSBO(lightClusterHost, lightClusterHost.numLights, currentSwapChainIdx);
        }
        else if (!m_dataToBePreProcessed.lightDeltaUpdates.empty() || m_dataToBePreProcessed.dirLightUpdated)
        {
            auto& lightClusterHost = *m_lightCluster;

            if (m_dataToBePreProcessed.dirLightUpdated)
            {
                const auto& dirLight = m_dataToBePreProcessed.dirLightUpdate;
                m_cachedDirLights = {dirLight};
                lightClusterHost.dirLight.direction =
                    mathstl::Vector4(dirLight.direction.x, dirLight.direction.y, dirLight.direction.z, 1);
                lightClusterHost.dirLight.direction.Normalize();
                lightClusterHost.dirLight.color =
                    mathstl::Vector4(dirLight.color.x, dirLight.color.y, dirLight.color.z, dirLight.color.w);
            }

            u32 dirtyMin = ~0u;
            u32 dirtyMax = 0;
            for (const auto& update : m_dataToBePreProcessed.lightDeltaUpdates)
            {
                if (update.index < MAX_SCENE_LIGHTS)
                {
                    lightClusterHost.lights[update.index] = update.light;
                    dirtyMin = (stltype::min)(dirtyMin, update.index);
                    dirtyMax = (stltype::max)(dirtyMax, update.index);
                }
            }

            if (m_dataToBePreProcessed.dirLightUpdated && m_dataToBePreProcessed.lightDeltaUpdates.empty())
            {
                // Transfer header only (dirLight + numLights + padding)
                DispatchSSBOTransfer((void*)m_lightCluster.get(),
                                     nullptr,
                                     (u32)UBO::LightClusterHeaderSize,
                                     &m_lightClusterSSBO,
                                     0,
                                     s_tileArrayBindingSlot,
                                     currentSwapChainIdx);
            }
            else if (dirtyMin <= dirtyMax)
            {
                static constexpr u64 headerSize = UBO::LightClusterHeaderSize;
                static constexpr u64 lightsOffsetInSSBO = UBO::LightClusterLightsOffset;

                const u32 rangeCount = dirtyMax - dirtyMin + 1;
                const u32 byteOffsetInLights = dirtyMin * sizeof(RenderLight);
                const u32 byteSizeRequested = rangeCount * sizeof(RenderLight);

                if (m_dataToBePreProcessed.dirLightUpdated)
                {
                    // Transfer header + up to the last dirty light
                    DispatchSSBOTransfer((void*)m_lightCluster.get(),
                                         nullptr,
                                         headerSize,
                                         &m_lightClusterSSBO,
                                         0,
                                         s_tileArrayBindingSlot,
                                         currentSwapChainIdx);

                    const u32 totalLightsSize = (dirtyMax + 1) * sizeof(RenderLight);
                    DispatchSSBOTransfer((void*)m_lightCluster->lights.data(),
                                         nullptr,
                                         totalLightsSize,
                                         &m_lightClusterSSBO,
                                         lightsOffsetInSSBO,
                                         s_tileArrayBindingSlot,
                                         currentSwapChainIdx);
                }
                else
                {
                    // Transfer just the dirty range of lights
                    DispatchSSBOTransfer((void*)(m_lightCluster->lights.data() + dirtyMin),
                                         nullptr,
                                         byteSizeRequested,
                                         &m_lightClusterSSBO,
                                         lightsOffsetInSSBO + byteOffsetInLights,
                                         s_tileArrayBindingSlot,
                                         currentSwapChainIdx);
                }
            }
        }

        constexpr bool isCullingEnabled = true;
        const bool isCullingFrozen = mathstl::isFlagSet(renderState.debugFlags, static_cast<u32>(DebugFlags::FreezeFrustumCulling));
        const u32 cascadeCount = (m_currentSharedDataUBO.cascadeCount > 0) ? static_cast<u32>(m_currentSharedDataUBO.cascadeCount) : 0u;

        m_cpuFrustumCulling.Execute(pPassManager->GetResourceManager(),
                                    frameViewProj,
                                    0,
                                    isCullingEnabled,
                                    isCullingFrozen,
                                    m_cachedTransformSSBO,
                                    cascadeCount == 0,
                                    currentSwapChainIdx);

        for (u32 c = 0; c < cascadeCount; ++c)
        {
            m_cpuFrustumCulling.Execute(pPassManager->GetResourceManager(),
                                        m_currentSharedDataUBO.csmViewMatrices[c],
                                        1 + c,
                                        isCullingEnabled,
                                        isCullingFrozen,
                                        m_cachedTransformSSBO,
                                        c == (cascadeCount - 1),
                                        currentSwapChainIdx);
        }

        // Clear
        m_needsToPropagateMainDataUpdate = true;
        m_frameIdxToPropagate = currentSwapChainIdx;
        m_dataToBePreProcessed.Clear();
        g_pQueueHandler->DispatchAllRequests();
    }

    m_frameRendererContexts[currentSwapChainIdx].numLights = m_lightCluster->numLights;
}

void FrameResourceManager::SetEntityMeshDataForFrame(EntityMeshDataMap&& data, u32 frameIdx)
{
    m_passDataMutex.lock();
    m_dataToBePreProcessed.entityMeshData = std::move(data);
    m_dataToBePreProcessed.frameIdx = frameIdx;
    m_passDataMutex.unlock();
}

void FrameResourceManager::SetEntityTransformDataForFrame(TransformSystemData&& data, u32 frameIdx)
{
    m_passDataMutex.lock();
    m_dataToBePreProcessed.entityTransformData = std::move(data);
    m_dataToBePreProcessed.frameIdx = frameIdx;
    m_passDataMutex.unlock();
}

void FrameResourceManager::SetLightDataForFrame(PointLightVector&& data, DirLightVector&& dirLights, u32 frameIdx)
{
    m_passDataMutex.lock();
    m_dataToBePreProcessed.lightVector = std::move(data);
    m_dataToBePreProcessed.dirLightVector = std::move(dirLights);
    m_dataToBePreProcessed.lightDeltaUpdates.clear();
    m_dataToBePreProcessed.dirLightUpdated = false;
    m_dataToBePreProcessed.frameIdx = frameIdx;
    m_passDataMutex.unlock();
}

void FrameResourceManager::SetLightDeltaForFrame(stltype::vector<LightDeltaUpdate>&& updates,
                                                 bool dirLightDirty,
                                                 const DirectionalRenderLight& dirLight,
                                                 u32 frameIdx)
{
    m_passDataMutex.lock();
    m_dataToBePreProcessed.lightDeltaUpdates = std::move(updates);
    m_dataToBePreProcessed.dirLightUpdated = dirLightDirty;
    m_dataToBePreProcessed.dirLightUpdate = dirLight;
    m_dataToBePreProcessed.frameIdx = frameIdx;
    m_passDataMutex.unlock();
}

void FrameResourceManager::SetSharedData(RenderView&& mainView, u32 frameIdx)
{
    m_passDataMutex.lock();
    if (mainView.zFar > 0.0f)
    {
        m_cachedMainView = mainView;
    }
    m_dataToBePreProcessed.mainView = std::move(mainView);
    m_dataToBePreProcessed.frameIdx = frameIdx;
    m_passDataMutex.unlock();
}

void FrameResourceManager::UpdateLightClusterSSBO(const UBO::LightClusterSSBO& data, u32 numLights, u32 frameIdx)
{
    // Memory mapping is handled at Init
    // Transfer header
    DispatchSSBOTransfer((void*)&data,
                         nullptr,
                         (u32)UBO::LightClusterHeaderSize,
                         &m_lightClusterSSBO,
                         0,
                         s_tileArrayBindingSlot,
                         frameIdx);

    // Transfer active lights data directly from the vector's data pointer
    if (numLights > 0)
    {
        DispatchSSBOTransfer((void*)data.lights.data(),
                             nullptr,
                             (u32)numLights * sizeof(RenderLight),
                             &m_lightClusterSSBO,
                             (u32)UBO::LightClusterLightsOffset,
                             s_tileArrayBindingSlot,
                             frameIdx);
    }
}

void FrameResourceManager::DispatchSSBOTransfer(void* data,
                                                DescriptorSet::Ptr pDescriptor,
                                                u32 size,
                                                StorageBuffer* pSSBO,
                                                u32 offset,
                                                u32 dstBinding,
                                                u32 frameIdx)
{
    AsyncQueueHandler::SSBOTransfer transfer;
    transfer.pData = data;
    transfer.size = size;
    transfer.offset = offset;
    transfer.pDescriptor = nullptr;
    transfer.pSSBO = pSSBO;
    transfer.dstBinding = dstBinding;
    transfer.frameIdx = frameIdx;
    g_pQueueHandler->SubmitTransferCommandAsync(transfer);
}

void FrameResourceManager::ClearGeometryCaches()
{
    SimpleScopedGuard lock(m_passDataMutex);
    m_currentPassGeometryState = PassGeometryData{};
    m_dataToBePreProcessed.Clear();
    m_cachedMainView = RenderView{};
    m_entityToTransformUBOIdx.clear();
    m_entityToObjectDataIdx.clear();
    m_cachedTransformSSBO.clear();
    m_cachedPrevTransformSSBO.clear();
    m_cachedSceneAABBs.clear();
    m_transformsToPropagateToPrev.clear();
    m_transformsPendingPrevCatchup.clear();
    m_cachedDirLights.clear();
    m_needsToPropagateMainDataUpdate = false;
    m_frameIdxToPropagate = 0;
}

} // namespace RenderPasses
