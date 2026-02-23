#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/AABB.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/Defines/LightDefines.h"
#include "Core/Global/ThreadBase.h"
#include "Core/Rendering/Core/Material.h"
#include "Core/Rendering/Core/View.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "Core/Rendering/Vulkan/VkSynchronization.h"
#include "Core/Rendering/Vulkan/VkDescriptorSetLayout.h"
#include "Core/Rendering/Core/Defines/UBODefines.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Vulkan/VkBuffer.h"
#include "Core/Rendering/Vulkan/VkDescriptorPool.h"
#include <EASTL/bitset.h>
#include <EASTL/vector.h>
#include <EASTL/hash_map.h>
#include <EASTL/fixed_vector.h>

class SharedResourceManager;

namespace RenderPasses
{

class PassManager;

struct EntityMeshData
{
    Mesh* pMesh;
    Material* pMaterial;
    class AABB aabb;
    // Needed for creating the instanced to draw in the renderpasses
    // Only guaranteed to be valid during Renderpass RebuildInternalData
    MeshHandle meshResourceHandle;
    u32 instanceDataIdx;

    EntityMeshData(Mesh* pM, Material* pMat, const class AABB& box, bool isDebug) : pMesh(pM), pMaterial(pMat), aabb{box}
    {
        flags[s_isDebugMeshFlag] = isDebug;
    }

    bool IsDebugMesh() const
    {
        return flags[s_isDebugMeshFlag] || flags[s_isDebugWireframeMesh];
    }
    bool IsInstanced() const
    {
        return flags[s_isInstancedFlag];
    }
    bool IsDebugWireframeMesh() const
    {
        return flags[s_isDebugWireframeMesh];
    }
    bool SetDebugMesh()
    {
        return flags[s_isDebugMeshFlag] = true;
    }
    bool SetInstanced()
    {
        return flags[s_isInstancedFlag] = true;
    }
    bool SetDebugWireframeMesh()
    {
        return flags[s_isDebugWireframeMesh] = true;
    }

    bool DidGeometryChange(const EntityMeshData& other) const
    {
        return pMesh != other.pMesh;
    }

protected:
    static inline u8 s_isDebugMeshFlag = 0;
    static inline u8 s_isInstancedFlag = 1;
    static inline u8 s_isDebugWireframeMesh = 2;
    stltype::bitset<8> flags{};
};

// Generic struct that stores the data by a pass to render the mesh (e.g. the
// static mesh pass)
struct PassMeshData
{
    EntityMeshData meshData;
    // Index into the transform UBO
    u32 transformIdx;
    u32 perObjectDataIdx;
};

struct PassGeometryData
{
    stltype::vector<PassMeshData> staticMeshPassData;
};

using EntityMeshDataMap = stltype::hash_map<u64, stltype::vector<EntityMeshData>>;
using EntityDebugMeshDataMap = stltype::hash_map<u64, EntityMeshData>;
using TransformSystemData = stltype::hash_map<ECS::EntityID, mathstl::Matrix>;
using EntityTransformData = stltype::pair<stltype::vector<ECS::EntityID>, stltype::vector<DirectX::XMFLOAT4X4>>;
using PointLightVector = stltype::vector<RenderLight>;
using DirLightVector = stltype::vector<DirectionalRenderLight>;
using EntityMaterialMap = stltype::hash_map<u64, stltype::vector<Material*>>;

struct FrameRendererContext
{
    // Timeline semaphore for graphics queue synchronization (wait on acquire, signal for present)
    TimelineSemaphore frameTimeline{};
    u64 nextTimelineValue{1};

    // Separate timeline for the async compute group
    TimelineSemaphore computeTimeline{};
    u64 nextComputeTimelineValue{1};
    u64 nextSSSComputeTimelineValue{1};

    // Signaled when the swapchain image is transitioned to the present layout, waited on by present
    Semaphore pPresentLayoutTransitionSignalSemaphore{};

    // Swapchain texture to render into for final presentation
    Texture* pCurrentSwapchainTexture{nullptr};

    DescriptorSet* shadowViewUBODescriptor{nullptr};
    DescriptorSet* tileArraySSBODescriptor{nullptr};
    DescriptorSet* sharedDataUBODescriptor{nullptr};
    DescriptorSet* gbufferPostProcessDescriptor{nullptr};

    // Shadow view UBO pointers for passes to update
    UniformBuffer* pShadowViewUBO{nullptr};
    GPUMappedMemoryHandle pMappedShadowViewUBO{nullptr};

    // Cluster grid SSBO (managed by PassManager, populated by SClusterAABB)
    StorageBuffer* pClusterGridBuffer{nullptr};
    DescriptorSet* clusterGridDescriptor{nullptr};

    Semaphore* renderingFinishedSemaphore{nullptr};
    Fence* renderingFinishedFence{nullptr};

    u32 imageIdx{0};
    u32 currentFrame{0};

    ::SharedResourceManager* pResourceManager{nullptr};

    f32 zNear{0.1f};
    f32 zFar{300.0f};
};

struct RenderDataForPreProcessing
{
    EntityMeshDataMap entityMeshData{};
    EntityMaterialMap entityMaterialData{};
    TransformSystemData entityTransformData{};
    PointLightVector lightVector{};
    DirLightVector dirLightVector{};
    RenderView mainView{mathstl::Vector3::Zero, mathstl::Vector3::Zero, {}, nullptr, 60.0f, 0.1f, 300.0f};
    u32 frameIdx{99};

    bool IsValid() const
    {
        return entityMeshData.size() <= entityTransformData.size() && frameIdx != 99;
    }
    bool IsEmpty() const
    {
        return entityMeshData.size() == 0 && entityTransformData.size() == 0 && lightVector.size() == 0 &&
               entityMaterialData.size() == 0 && dirLightVector.size() == 0;
    }

    void Clear()
    {
        entityMeshData.clear();
        entityTransformData.clear();
        lightVector.clear();
        dirLightVector.clear();
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

    void PreProcessDataForCurrentFrame(u32 frameIdx, u32 currentSwapChainIdx, PassManager* pPassManager);

    void SetEntityMeshDataForFrame(EntityMeshDataMap&& data, u32 frameIdx);
    void SetEntityTransformDataForFrame(TransformSystemData&& data, u32 frameIdx);
    void SetLightDataForFrame(PointLightVector&& data, DirLightVector&& dirLights, u32 frameIdx);
    void SetSharedData(RenderView&& mainView, u32 frameIdx);

    void UpdateSharedDataUBO(const void* data, size_t size, u32 frameIdx);
    void UpdateShadowViewUBO(const UBO::ShadowmapViewUBO& data, u32 frameIdx);
    void UpdateLightClusterSSBO(const UBO::LightClusterSSBO& data, u32 frameIdx);

    void DispatchSSBOTransfer(
        void* data, DescriptorSet* pDescriptor, u32 size, StorageBuffer* pSSBO, u32 offset = 0, u32 dstBinding = 0);

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
    UniformBuffer& GetShadowViewUBO() { return m_shadowViewUBO; }
    StorageBuffer& GetClusterGridSSBO() { return m_clusterGridSSBO; }

    GPUMappedMemoryHandle GetMappedSharedDataUBOBuffer() const { return m_mappedSharedDataUBOBuffer; }
    GPUMappedMemoryHandle GetMappedLightUniformsUBO() const { return m_mappedLightUniformsUBO; }
    GPUMappedMemoryHandle GetMappedGBufferPostProcessUBO() const { return m_mappedGBufferPostProcessUBO; }
    GPUMappedMemoryHandle GetMappedShadowMapUBO() const { return m_mappedShadowMapUBO; }
    GPUMappedMemoryHandle GetMappedShadowViewUBO() const { return m_mappedShadowViewUBO; }

    UBO::LightClusterSSBO& GetLightCluster() { return m_lightCluster; }
    ShadowMapState& GetShadowMapState() { return m_currentShadowMapState; }


private:
    void BuildSharedDataForView(const RenderView& mainView,
                                UBO::SharedDataUBO& ubo,
                                mathstl::Matrix& viewMat,
                                mathstl::Matrix& viewProj) const;

    ProfiledLockable(CustomMutex, m_passDataMutex);
    RenderDataForPreProcessing m_dataToBePreProcessed;

    // Global buffers
    StorageBuffer m_lightClusterSSBO;
    StorageBuffer m_clusterGridSSBO;
    UniformBuffer m_sharedDataUBO;
    UniformBuffer m_lightUniformsUBO;
    UniformBuffer m_gbufferPostProcessUBO;
    UniformBuffer m_shadowMapUBO;
    UniformBuffer m_shadowViewUBO;
    UBO::LightClusterSSBO m_lightCluster;

    GPUMappedMemoryHandle m_mappedSharedDataUBOBuffer{nullptr};
    GPUMappedMemoryHandle m_mappedLightUniformsUBO{nullptr};
    GPUMappedMemoryHandle m_mappedGBufferPostProcessUBO{nullptr};
    GPUMappedMemoryHandle m_mappedShadowMapUBO{nullptr};
    GPUMappedMemoryHandle m_mappedShadowViewUBO{nullptr};

    DescriptorPool m_descriptorPool;
    DescriptorSetLayout m_clusterGridSSBOLayout;
    DescriptorSetLayoutVulkan m_lightClusterSSBOLayout;
    DescriptorSetLayoutVulkan m_sharedDataUBOLayout;
    DescriptorSetLayoutVulkan m_shadowViewUBOLayout;
    DescriptorSetLayout m_gbufferPostProcessLayout;

    stltype::fixed_vector<FrameRendererContext, SWAPCHAIN_IMAGES> m_frameRendererContexts{SWAPCHAIN_IMAGES};
    
    PassGeometryData m_currentPassGeometryState{};
    stltype::hash_map<ECS::EntityID, u32> m_entityToTransformUBOIdx{};
    stltype::hash_map<ECS::EntityID, u32> m_entityToObjectDataIdx{};
    DirLightVector m_cachedDirLights{};
    stltype::vector<DirectX::XMFLOAT4X4> m_cachedTransformSSBO{};
    stltype::vector<class AABB> m_cachedSceneAABBs{};

    bool m_needsToPropagateMainDataUpdate{false};
    u32 m_frameIdxToPropagate{0};

    ShadowMapState m_currentShadowMapState{};
};

} // namespace RenderPasses
