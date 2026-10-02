#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/CpuPreprocess/CpuFrustumCulling.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/Defines/LightDefines.h"
#include "Core/Global/ThreadBase.h"
#include "Core/Rendering/Core/View.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Core/DescriptorSetLayout.h"
#include "Core/Rendering/Core/Defines/UBODefines.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/Buffer.h"
#include "Core/Rendering/Core/MappedUniformBuffer.h"
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

struct FrameRendererContext
{
    TimelineSemaphore frameTimeline{};
    TimelineSemaphore computeTimeline{};
    Semaphore pPresentLayoutTransitionSignalSemaphore{};
    Semaphore* renderingFinishedSemaphore{nullptr};

    Texture* pCurrentSwapchainTexture{nullptr};

    DescriptorSet::Ptr tileArraySSBODescriptor{nullptr};
    DescriptorSet::Ptr sharedDataUBODescriptor{nullptr};
    DescriptorSet::Ptr gbufferPostProcessDescriptor{nullptr};
    DescriptorSet::Ptr clusterGridDescriptor{nullptr};

    u32 currentFrame{0};
    // Monotonic, unlike currentFrame which only cycles through the frame slots
    u64 frameCounter{0};

    ::SharedResourceManager* pResourceManager{nullptr};

    f32 zNear{0.1f};
    f32 zFar{300.0f};
    // Vertical field of view in degrees
    f32 fovY{0.0f};
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
                                     stltype::fixed_vector<Fence, SWAPCHAIN_IMAGES>& imageAvailableFences);

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

    void UpdateLightClusterSSBO(const UBO::LightClusterSSBO& data);

    void DispatchSSBOTransfer(const void* data, u32 size, StorageBuffer* pSSBO, u32 offset = 0);

    FrameRendererContext& GetFrameRendererContext(u32 idx) { return m_frameRendererContexts[idx]; }

    MappedUniformBuffer<UBO::GBufferPostProcessUBO>& GetGBufferPostProcessUBO() { return m_gbufferPostProcessUBO; }
    ShadowMapState& GetShadowMapState() { return m_currentShadowMapState; }
    const PassGeometryData& GetCurrentPassGeometryState() const { return m_currentPassGeometryState; }
    const DirectX::XMFLOAT4X4& GetCurrentTransform(u32 idx) const { return m_cachedTransformSSBO[idx]; }

private:
    void BuildSharedDataForView(const RenderView& mainView,
                                const mathstl::Vector2& renderResolution,
                                const mathstl::Vector2& outputResolution,
                                u64 jitterFrameNumber,
                                UBO::SharedDataUBO& ubo) const;

    ProfiledLockable(CustomMutex, m_passDataMutex);
    RenderDataForPreProcessing m_dataToBePreProcessed;
    RenderView m_cachedMainView{};

    StorageBuffer m_lightClusterSSBO;
    StorageBuffer m_clusterGridSSBO;
    MappedUniformBuffer<UBO::SharedDataUBO> m_sharedDataUBO;
    MappedUniformBuffer<LightUniforms> m_lightUniformsUBO;
    MappedUniformBuffer<UBO::GBufferPostProcessUBO> m_gbufferPostProcessUBO;
    MappedUniformBuffer<UBO::ShadowMapUBO> m_shadowMapUBO;
    stltype::unique_ptr<UBO::LightClusterSSBO> m_lightCluster;

    DescriptorPool m_descriptorPool;
    DescriptorSetLayout m_clusterGridSSBOLayout;
    DescriptorSetLayout m_lightClusterSSBOLayout;
    DescriptorSetLayout m_sharedDataUBOLayout;
    DescriptorSetLayout m_gbufferPostProcessLayout;

    stltype::fixed_vector<FrameRendererContext, SWAPCHAIN_IMAGES> m_frameRendererContexts =
        stltype::fixed_vector<FrameRendererContext, SWAPCHAIN_IMAGES>(SWAPCHAIN_IMAGES);

    UBO::SharedDataUBO m_currentSharedDataUBO{};
    PassGeometryData m_currentPassGeometryState{};
    stltype::hash_map<ECS::EntityID, u32> m_entityToTransformUBOIdx{};
    u32 m_firstNewTransformSlot{0};
    DirLightVector m_cachedDirLights{};
    stltype::vector<DirectX::XMFLOAT4X4> m_cachedTransformSSBO{};
    stltype::vector<DirectX::XMFLOAT4X4> m_cachedPrevTransformSSBO{};
    stltype::vector<AABB> m_cachedSceneAABBs{};
    stltype::vector<u32> m_transformsToPropagateToPrev{};
    stltype::vector<u32> m_transformsPendingPrevCatchup{};

    RenderingCore::CpuFrustumCulling m_cpuFrustumCulling{};

    ShadowMapState m_currentShadowMapState{};

    TextureHandle m_skyboxTextureHandle{0};
    BindlessTextureHandle m_skyboxBindlessHandle{0};
};

} // namespace RenderPasses
