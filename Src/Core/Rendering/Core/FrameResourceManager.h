#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/Defines/LightDefines.h"
#include "Core/Global/ThreadBase.h"
#include "Core/Rendering/Core/View.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Vulkan/VkSynchronization.h"
#include "Core/Rendering/Vulkan/VkDescriptorSetLayout.h"
#include "Core/Rendering/Core/Defines/UBODefines.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Vulkan/VkBuffer.h"
#include "Core/Rendering/Core/DescriptorPool.h"
#include "Core/Rendering/Passes/PassManagerDefines.h"
#include <EASTL/fixed_vector.h>
#include <EASTL/unique_ptr.h>

class SharedResourceManager;

namespace RenderPasses
{

class PassManager;

using PointLightVector = stltype::vector<RenderLight>;
using DirLightVector   = stltype::vector<DirectionalRenderLight>;

struct LightDeltaUpdate
{
    u32 index;
    RenderLight light;
};

struct FrameCameraData
{
    mathstl::Matrix viewToClip{mathstl::Matrix::Identity};
    mathstl::Matrix clipToView{mathstl::Matrix::Identity};
    mathstl::Matrix clipToPrevClip{mathstl::Matrix::Identity};
    mathstl::Matrix prevClipToClip{mathstl::Matrix::Identity};
    mathstl::Vector2 jitterOffset{mathstl::Vector2::Zero};
    mathstl::Vector3 position{mathstl::Vector3::Zero};
    mathstl::Vector3 up{0.0f, 1.0f, 0.0f};
    mathstl::Vector3 right{1.0f, 0.0f, 0.0f};
    mathstl::Vector3 forward{0.0f, 0.0f, -1.0f};
    f32 fovRadians{0.0f};
    f32 aspectRatio{1.0f};
    f32 nearPlane{0.1f};
    f32 farPlane{300.0f};
};

struct FrameRendererContext
{
    TimelineSemaphore frameTimeline{};
    u64 nextTimelineValue{0};

    TimelineSemaphore computeTimeline{};
    u64 nextComputeTimelineValue{0};
    u64 nextSSSComputeTimelineValue{0};

    Semaphore pPresentLayoutTransitionSignalSemaphore{};

    Texture* pCurrentSwapchainTexture{nullptr};

    DescriptorSet::Ptr tileArraySSBODescriptor{nullptr};
    DescriptorSet::Ptr sharedDataUBODescriptor{nullptr};
    DescriptorSet::Ptr gbufferPostProcessDescriptor{nullptr};

    GPUMappedMemoryHandle pMappedSharedDataUBO{nullptr};

    StorageBuffer* pClusterGridBuffer{nullptr};
    DescriptorSet::Ptr clusterGridDescriptor{nullptr};

    Semaphore* renderingFinishedSemaphore{nullptr};
    Fence* renderingFinishedFence{nullptr};

    u32 imageIdx{0};
    u32 currentFrame{0};

    ::SharedResourceManager* pResourceManager{nullptr};

    FrameCameraData cameraData{};
    Texture* pDLSSExposureTexture{nullptr};
    f32 zNear{0.1f};
    f32 zFar{300.0f};
    u32 numLights{0};
};

struct RenderDataForPreProcessing
{
    EntityMeshDataMap entityMeshData{};
    EntityMaterialMap entityMaterialData{};
    TransformSystemData entityTransformData{};
    PointLightVector lightVector{};
    DirLightVector dirLightVector{};
    stltype::vector<LightDeltaUpdate> lightDeltaUpdates{};
    DirectionalRenderLight dirLightUpdate{};
    bool dirLightUpdated{false};
    RenderView mainView{mathstl::Vector3::Zero, mathstl::Vector3::Zero, {}, nullptr, 60.0f, 0.1f, 300.0f};
    u32 frameIdx{99};

    bool IsValid() const
    {
        return frameIdx != 99;
    }
    bool IsEmpty() const
    {
        return entityMeshData.size() == 0 && entityTransformData.size() == 0 && lightVector.size() == 0 &&
               entityMaterialData.size() == 0 && dirLightVector.size() == 0 && lightDeltaUpdates.size() == 0 &&
               !dirLightUpdated;
    }

    void Clear()
    {
        entityMeshData.clear();
        entityTransformData.clear();
        lightVector.clear();
        dirLightVector.clear();
        lightDeltaUpdates.clear();
        dirLightUpdated = false;
        entityMaterialData.clear();
        frameIdx = 99;
    }
};

class FrameResourceManager
{
public:
    struct ShadowMapState
    {
        u32 cascadeCount{};
        mathstl::Vector2 shadowMapExtents{};
    };

    void Init();
    void CreatePassObjectsAndLayouts();
    void CreateFrameRendererContexts(stltype::fixed_vector<Semaphore, SWAPCHAIN_IMAGES>& imageAvailableSemaphores,
                                     stltype::fixed_vector<Fence, SWAPCHAIN_IMAGES>& imageAvailableFences,
                                     stltype::fixed_vector<Fence, SWAPCHAIN_IMAGES>& renderFinishedFences);

    void PreProcessDataForCurrentFrame(u32 frameIdx,
                                       u64 jitterFrameNumber,
                                       u32 currentSwapChainIdx,
                                       PassManager* pPassManager);

    void ClearGeometryCaches();

    void SetEntityMeshDataForFrame(EntityMeshDataMap&& data, u32 frameIdx);
    void SetEntityTransformDataForFrame(TransformSystemData&& data, u32 frameIdx);
    void SetLightDataForFrame(PointLightVector&& data, DirLightVector&& dirLights, u32 frameIdx);
    void SetLightDeltaForFrame(stltype::vector<LightDeltaUpdate>&& updates, bool dirLightDirty,
                               const DirectionalRenderLight& dirLight, u32 frameIdx);
    void SetSharedData(RenderView&& mainView, u32 frameIdx);

    void UpdateSharedDataUBO(const void* data, size_t size, u32 frameIdx);
    void UpdateLightClusterSSBO(const UBO::LightClusterSSBO& data, u32 numLights, u32 frameIdx);

    void DispatchSSBOTransfer(
        void* data, DescriptorSet::Ptr pDescriptor, u32 size, StorageBuffer* pSSBO, u32 offset = 0, u32 dstBinding = 0, u32 frameIdx = ~0u);

    FrameRendererContext& GetFrameRendererContext(u32 idx) { return m_frameRendererContexts[idx]; }
    const FrameRendererContext& GetFrameRendererContext(u32 idx) const { return m_frameRendererContexts[idx]; }

    RenderDataForPreProcessing& GetDataToBePreProcessed() { return m_dataToBePreProcessed; }
    void LockData() { m_passDataMutex.lock(); }
    void UnlockData() { m_passDataMutex.unlock(); }

    StorageBuffer& GetLightClusterSSBO() { return m_lightClusterSSBO; }
    UniformBuffer& GetSharedDataUBO() { return m_sharedDataUBO; }
    UniformBuffer& GetLightUniformsUBO() { return m_lightUniformsUBO; }
    UniformBuffer& GetGBufferPostProcessUBO() { return m_gbufferPostProcessUBO; }
    UniformBuffer& GetShadowMapUBO() { return m_shadowMapUBO; }
    StorageBuffer& GetClusterGridSSBO() { return m_clusterGridSSBO; }

    GPUMappedMemoryHandle GetMappedSharedDataUBOBuffer() const { return m_mappedSharedDataUBOBuffer; }
    GPUMappedMemoryHandle GetMappedLightUniformsUBO() const { return m_mappedLightUniformsUBO; }
    GPUMappedMemoryHandle GetMappedGBufferPostProcessUBO() const { return m_mappedGBufferPostProcessUBO; }
    GPUMappedMemoryHandle GetMappedShadowMapUBO() const { return m_mappedShadowMapUBO; }

    UBO::LightClusterSSBO& GetLightCluster() { return *m_lightCluster; }
    ShadowMapState& GetShadowMapState() { return m_currentShadowMapState; }
    const PassGeometryData& GetCurrentPassGeometryState() const { return m_currentPassGeometryState; }
    const DirectX::XMFLOAT4X4& GetCurrentTransform(u32 idx) const { return m_cachedTransformSSBO[idx]; }

private:
    void BuildSharedDataForView(const RenderView& mainView,
                                const mathstl::Vector2& renderResolution,
                                u64 jitterFrameNumber,
                                UBO::SharedDataUBO& ubo,
                                mathstl::Matrix& viewMat,
                                mathstl::Matrix& viewProj,
                                mathstl::Vector2& jitter,
                                FrameCameraData& cameraData) const;

    ProfiledLockable(CustomMutex, m_passDataMutex);
    RenderDataForPreProcessing m_dataToBePreProcessed;

    StorageBuffer m_lightClusterSSBO;
    StorageBuffer m_clusterGridSSBO;
    UniformBuffer m_sharedDataUBO;
    UniformBuffer m_lightUniformsUBO;
    UniformBuffer m_gbufferPostProcessUBO;
    UniformBuffer m_shadowMapUBO;
    stltype::unique_ptr<UBO::LightClusterSSBO> m_lightCluster;

    GPUMappedMemoryHandle m_mappedSharedDataUBOBuffer{nullptr};
    GPUMappedMemoryHandle m_mappedLightUniformsUBO{nullptr};
    GPUMappedMemoryHandle m_mappedGBufferPostProcessUBO{nullptr};
    GPUMappedMemoryHandle m_mappedShadowMapUBO{nullptr};

    DescriptorPool m_descriptorPool;
    DescriptorSetLayout m_clusterGridSSBOLayout;
    DescriptorSetLayoutVulkan m_lightClusterSSBOLayout;
    DescriptorSetLayoutVulkan m_sharedDataUBOLayout;
    DescriptorSetLayout m_gbufferPostProcessLayout;

    stltype::fixed_vector<FrameRendererContext, SWAPCHAIN_IMAGES> m_frameRendererContexts =
        stltype::fixed_vector<FrameRendererContext, SWAPCHAIN_IMAGES>(SWAPCHAIN_IMAGES);

    UBO::SharedDataUBO m_currentSharedDataUBO{};
    PassGeometryData m_currentPassGeometryState{};
    stltype::hash_map<ECS::EntityID, u32> m_entityToTransformUBOIdx{};
    stltype::hash_map<ECS::EntityID, u32> m_entityToObjectDataIdx{};
    DirLightVector m_cachedDirLights{};
    stltype::vector<DirectX::XMFLOAT4X4> m_cachedTransformSSBO{};
    stltype::vector<DirectX::XMFLOAT4X4> m_cachedPrevTransformSSBO{};
    stltype::vector<AABB> m_cachedSceneAABBs{};
    stltype::vector<u32> m_transformsToPropagateToPrev{};
    stltype::vector<u32> m_transformsPendingPrevCatchup{};

    bool m_needsToPropagateMainDataUpdate{false};
    u32 m_frameIdxToPropagate{0};

    u32 m_framesToRebuild{0};

    ShadowMapState m_currentShadowMapState{};
};

} // namespace RenderPasses
