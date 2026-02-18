#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/AABB.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/Defines/LightDefines.h"
#include "Core/Rendering/Vulkan/VkGPUTimingQuery.h"


#include "Core/Events/EventSystem.h"
#include "Core/Global/ThreadBase.h"
#include "Core/Rendering/Core/Material.h"
#include "Core/Rendering/Core/Pipeline.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/ShadowMaps.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/View.h"
#include "Core/Rendering/Vulkan/VkAttachment.h"
#include "Core/SceneGraph/Mesh.h"
#include "Core/WindowManager.h"
#include "GBuffer.h"
#include "LightResourceManager.h"

class SharedResourceManager;

namespace RenderPasses
{
class ConvolutionRenderPass;

struct EntityMeshData
{
    Mesh* pMesh;
    Material* pMaterial;
    AABB aabb;
    // Needed for creating the instanced to draw in the renderpasses
    // Only guaranteed to be valid during Renderpass RebuildInternalData
    MeshHandle meshResourceHandle;
    u32 instanceDataIdx;

    EntityMeshData(Mesh* pM, Material* pMat, const AABB& box, bool isDebug) : pMesh(pM), pMaterial(pMat), aabb{box}
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

using EntityMeshDataMap = stltype::hash_map<u64, stltype::vector<EntityMeshData>>;
using EntityDebugMeshDataMap = stltype::hash_map<u64, EntityMeshData>;
using TransformSystemData = stltype::hash_map<ECS::EntityID, mathstl::Matrix>;
using EntityTransformData = stltype::pair<stltype::vector<ECS::EntityID>, stltype::vector<DirectX::XMFLOAT4X4>>;
using PointLightVector = stltype::vector<RenderLight>;
using DirLightVector = stltype::vector<DirectionalRenderLight>;
using EntityMaterialMap = stltype::hash_map<u64, stltype::vector<Material*>>;

struct EntityRenderData
{
    EntityMeshData meshData;
};

struct PassGeometryData
{
    stltype::vector<PassMeshData> staticMeshPassData;
};

// A pass is responsible for rendering a view (aka, main pass renders the main
// camera view), can also execute solely on CPU side (think culling would be a
// pass too) Won't make it too complex but pass data will be roughly sorted
// based on the view type
struct MainPassData
{
    SharedResourceManager* pResourceManager;
    GBuffer* pGbuffer;
    RenderView mainView;
    mathstl::Matrix mainCamViewMatrix;
    mathstl::Matrix mainCamInvViewProj;
    // Views we want to render with CSMs
    stltype::vector<CsmRenderView> csmViews;
    // Views we just render into normal shadowmaps whatever those will end up
    // being
    stltype::vector<RenderView> shadowViews;
    stltype::vector<DescriptorSet*> viewDescriptorSets;
    stltype::hash_map<UBO::DescriptorContentsType, DescriptorSet*> bufferDescriptors;
    CascadedShadowMap directionalLightShadowMap;
    u32 cascades;
    f32 csmStepSize;
};

enum class PassType
{
    PreProcess,
    Main,
    UI,
    Debug,
    Shadow,
    PostProcess,
    Composite
};

// ============================================================================
// PASS GROUP EXECUTION ORDER
// Defines the order in which pass groups are executed and synchronized.
// Each group uses a single command buffer and waits on the previous group.
// ============================================================================
inline constexpr PassType GROUP_EXECUTION_ORDER[] = {
    PassType::PreProcess, PassType::Main, PassType::Debug, PassType::Shadow, PassType::UI, PassType::Composite};
inline constexpr u32 GROUP_COUNT = sizeof(GROUP_EXECUTION_ORDER) / sizeof(PassType);

// Helper to get pass type name for logging
inline const char* GetPassTypeName(PassType type)
{
    switch (type)
    {
        case PassType::PreProcess:
            return "PreProcess";
        case PassType::Main:
            return "Main";
        case PassType::Debug:
            return "Debug";
        case PassType::Shadow:
            return "Shadow";
        case PassType::UI:
            return "UI";
        case PassType::Composite:
            return "Composite";
        case PassType::PostProcess:
            return "PostProcess";
        default:
            return "Unknown";
    }
}

struct PassGroupContext
{
    CommandPool cmdPool;
    CommandPool computeCmdPool;
    // Outer vector: swapchain images, Inner vector: command buffers for passes in this group
    stltype::fixed_vector<stltype::vector<CommandBuffer*>, SWAPCHAIN_IMAGES> cmdBuffers{SWAPCHAIN_IMAGES};
    stltype::fixed_vector<stltype::vector<CommandBuffer*>, SWAPCHAIN_IMAGES> computeCmdBuffers{SWAPCHAIN_IMAGES};
    // Per-pass queue type (indexed by pass order, skipping non-rendering passes)
    stltype::vector<QueueType> passQueueTypes{};
    bool initialized{false};

    void Reset(u32 imageIdx)
    {
        for (auto* cmdBuffer : cmdBuffers[imageIdx])
            static_cast<CBufferVulkan*>(cmdBuffer)->ResetBuffer();
        for (auto* cmdBuffer : computeCmdBuffers[imageIdx])
            static_cast<CBufferVulkan*>(cmdBuffer)->ResetBuffer();
    }
};


struct FrameRendererContext
{
    // Timeline semaphore for pass group synchronization
    TimelineSemaphore frameTimeline{};
    u64 nextTimelineValue{1}; // Reset to 1 each frame, 0 is initial value

    Semaphore pInitialLayoutTransitionSignalSemaphore{};
    // Signaled when the swapchain image is transitioned to the present layout
    Semaphore pPresentLayoutTransitionSignalSemaphore{};
    // Signaled when the passes that don't care about color attachment values are
    // finished rendering
    Semaphore nonLoadRenderingFinished{};
    Semaphore toReadTransitionFinished{};

    // Swapchain texture to render into for final presentation
    Texture* pCurrentSwapchainTexture{nullptr};

    DescriptorSet* shadowViewUBODescriptor;
    DescriptorSet* tileArraySSBODescriptor;
    DescriptorSet* mainViewUBODescriptor;
    DescriptorSet* gbufferPostProcessDescriptor;

    // Shadow view UBO pointers for passes to update
    UniformBuffer* pShadowViewUBO{nullptr};
    GPUMappedMemoryHandle pMappedShadowViewUBO{nullptr};

    // Cluster grid SSBO (managed by PassManager, populated by SClusterAABB)
    StorageBuffer* pClusterGridBuffer{nullptr};
    DescriptorSet* clusterGridDescriptor{nullptr};

    Semaphore* renderingFinishedSemaphore;
    Fence* renderingFinishedFence{nullptr};

    u32 imageIdx;
    u32 currentFrame;

    SharedResourceManager* pResourceManager{nullptr};

    f32 zNear{0.1f};
    f32 zFar{300.0f};
};

enum class ColorAttachmentType
{
    GBufferColor,
};

struct RendererAttachmentInfo
{
    GBufferInfo gbuffer;
    CascadedShadowMap directionalLightShadowMap;
    stltype::hash_map<ColorAttachmentType, stltype::vector<ColorAttachment>> colorAttachments;
    DepthBufferAttachmentVulkan depthAttachment;
};

struct InstancedMeshDataInfo
{
    u32 instanceCount;
    u32 indexBufferOffset;
    u32 instanceOffset;
    u32 verticesPerInstance;
    stltype::vector<PSO*> PSOs{};
};

class PassManager
{
public:
    PassManager()
    {
        g_pEventSystem->AddBaseInitEventCallback([&](const auto&) { Init(); });
    }
    ~PassManager();

    void Init();
    void RecreateGbuffers(const mathstl::Vector2& resolution);

    void AddPass(PassType type, stltype::unique_ptr<ConvolutionRenderPass>&& pass);
    void TransferPassData(const PassGeometryData& passData, u32 frameIdx);

    void ExecutePasses(u32 frameIdx);
    void ReadAndPublishTimingResults(u32 frameIdx);

    // Can be called from many different threads
    void SetEntityMeshDataForFrame(EntityMeshDataMap&& data, u32 frameIdx);
    void SetEntityTransformDataForFrame(TransformSystemData&& data, u32 frameIdx);
    void SetLightDataForFrame(PointLightVector&& data, DirLightVector&& dirLights, u32 frameIdx);

    void SetMainViewData(UBO::ViewUBO&& viewUBO, f32 zNear, f32 zFar, u32 frameIdx);

    void PreProcessDataForCurrentFrame(u32 frameIdx);

    void RegisterDebugCallbacks();

    void UpdateMainViewUBO(const void* data, size_t size, u32 frameIdx);
    void UpdateShadowViewUBO(const UBO::ShadowmapViewUBO& data, u32 frameIdx);
    void UpdateLightClusterSSBO(const UBO::LightClusterSSBO& data, u32 frameIdx);

    void DispatchSSBOTransfer(
        void* data, DescriptorSet* pDescriptor, u32 size, StorageBuffer* pSSBO, u32 offset = 0, u32 dstBinding = 0);
    void BlockUntilPassesFinished(u32 frameIdx);

    // Mainly used to hot reload shaders
    // Rebuilding all pipelines for all passes is not the most efficient but we
    // don't have that many and won't be doing it often anyway
    void RebuildPipelinesForAllPasses();

    // GPU timing query accessors
    const stltype::vector<PassTimingResult>& GetPassTimingResults() const;
    f32 GetTotalGPUTimeMs() const;

protected:
    void PreProcessMeshData(const stltype::vector<PassMeshData>& meshes, u32 lastFrame, u32 curFrame);

    void RecreateShadowMaps(u32 cascades, const mathstl::Vector2& extents);

    // Helpers to split large Init / ExecutePasses
    void InitResourceManagerAndCallbacks();
    void CreatePassObjectsAndLayouts();
    void CreateUBOsAndMap();
    void CreateFrameRendererContexts();
    void InitPassesAndImGui();

    bool AnyPassWantsToRender() const;
    void PrepareMainPassDataForFrame(MainPassData& mainPassData, FrameRendererContext& ctx, u32 frameIdx);
    void PerformInitialLayoutTransitions(FrameRendererContext& ctx,
                                         const stltype::vector<const Texture*>& gbufferTextures,
                                         Texture* pSwapChainTexture,
                                         Semaphore& imageAvailableSemaphore);
    void RenderAllPassGroups(const MainPassData& mainPassData,
                             FrameRendererContext& ctx,
                             Semaphore& imageAvailableSemaphore);
    bool RenderPassGroup(PassType groupType, const MainPassData& data, FrameRendererContext& ctx);
    void InitPassGroupContexts();

    // Layout transition helpers
    Semaphore* GetLastActiveGroupSemaphore(FrameRendererContext& ctx);
    void TransitionGBuffersToShaderRead(FrameRendererContext& ctx,
                                        const stltype::vector<const Texture*>& gbufferTextures);
    void TransitionUIToShaderRead(FrameRendererContext& ctx, const Texture* pUITexture);
    void TransitionSwapchainToColorAttachment(FrameRendererContext& ctx);
    void TransitionSwapchainToPresent(FrameRendererContext& ctx);

private:
    ProfiledLockable(CustomMutex, m_passDataMutex);

    // GPU timing query
    GPUTimingQuery m_gpuTimingQuery;

    // Cluster Grid Data
    StorageBuffer m_clusterGridSSBO;
    DescriptorSetLayout m_clusterGridSSBOLayout;

    // Resource Manager
    SharedResourceManager m_resourceManager;

    // Only need one gbuffer
    GBuffer m_gbuffer;

    // Pass data for each frame
    stltype::hash_map<PassType, stltype::vector<stltype::unique_ptr<ConvolutionRenderPass>>> m_passes{};
    stltype::fixed_vector<MainPassData, SWAPCHAIN_IMAGES> m_mainPassData{SWAPCHAIN_IMAGES};
    stltype::fixed_vector<FrameRendererContext, SWAPCHAIN_IMAGES> m_frameRendererContexts{};
    stltype::fixed_vector<Semaphore, SWAPCHAIN_IMAGES> m_imageAvailableSemaphores{SWAPCHAIN_IMAGES};
    stltype::fixed_vector<Fence, SWAPCHAIN_IMAGES> m_imageAvailableFences{SWAPCHAIN_IMAGES};
    // Fences to track when rendering to each swapchain image completes (for semaphore reuse safety)
    stltype::fixed_vector<Fence, SWAPCHAIN_IMAGES> m_renderFinishedFences{SWAPCHAIN_IMAGES};
    stltype::hash_map<PassType, PassGroupContext> m_passGroupContexts{};
    stltype::hash_map<ECS::EntityID, u32> m_entityToTransformUBOIdx{};
    stltype::hash_map<ECS::EntityID, u32> m_entityToObjectDataIdx{};

    // Current Geometry state, mainly used as a simple way to check for changes
    PassGeometryData m_currentPassGeometryState{};

    // Global transform SSBO
    StorageBuffer m_lightClusterSSBO;
    UniformBuffer m_viewUBO;
    UniformBuffer m_lightUniformsUBO;
    UniformBuffer m_gbufferPostProcessUBO;
    UniformBuffer m_shadowMapUBO;
    UniformBuffer m_shadowViewUBO;
    UBO::LightClusterSSBO m_lightCluster;
    GPUMappedMemoryHandle m_mappedViewUBOBuffer;
    GPUMappedMemoryHandle m_mappedLightUniformsUBO;
    GPUMappedMemoryHandle m_mappedGBufferPostProcessUBO;
    GPUMappedMemoryHandle m_mappedShadowMapUBO;
    GPUMappedMemoryHandle m_mappedShadowViewUBO;
    DescriptorPool m_descriptorPool;

    // Light cluster SSBO layout
    DescriptorSetLayoutVulkan m_lightClusterSSBOLayout;
    // Global view UBO layout
    DescriptorSetLayoutVulkan m_viewUBOLayout;
    // Needs separate layout since we update it during rendering of shadow passes
    // and previous passes shouldn't use the old data
    DescriptorSetLayoutVulkan m_shadowViewUBOLayout;
    DescriptorSetLayout m_gbufferPostProcessLayout;

    // Global attachment infos for all passes like gbuffer, depth buffer or
    // swapchain textures
    RendererAttachmentInfo m_globalRendererAttachments;

    // Depth texture created during Init and used later for attachments/ImGui
    Texture* m_pDepthTexture{nullptr};

    u32 m_currentSwapChainIdx;

    void CreateDepthAttachment();

    // struct to hold data for all meshes for the next frame to be rendered, uses
    // more memory but allows for async pre-processing and we shouldn't rebuild
    // data often anyway
    struct RenderDataForPreProcessing
    {
        EntityMeshDataMap entityMeshData{};
        EntityMaterialMap entityMaterialData{};
        TransformSystemData entityTransformData{};
        PointLightVector lightVector{};
        DirLightVector dirLightVector{};
        stltype::optional<UBO::ViewUBO> mainViewUBO{};
        f32 zNear{0.1f};
        f32 zFar{300.0f};
        u32 frameIdx{99};

        bool IsValid() const
        {
            return entityMeshData.size() <= entityTransformData.size() && frameIdx != 99;
        }
        bool IsEmpty() const
        {
            return entityMeshData.size() == 0 && entityTransformData.size() == 0 && lightVector.size() == 0 &&
                   entityMaterialData.size() == 0 && dirLightVector.size() == 0 && !mainViewUBO.has_value();
        }

        void Clear()
        {
            entityMeshData.clear();
            entityTransformData.clear();
            lightVector.clear();
            dirLightVector.clear();
            entityMaterialData.clear();
            mainViewUBO.reset();
            frameIdx = 99;
        }
    };
    RenderDataForPreProcessing m_dataToBePreProcessed;

    struct ShadowMapState
    {
        u32 cascadeCount{};
        mathstl::Vector2 shadowMapExtents{};
    };
    ShadowMapState m_currentShadowMapState{};

    u32 m_currentFrame{0};
    bool m_needsToPropagateMainDataUpdate{false};
    u32 m_frameIdxToPropagate{0};
    BindlessTextureHandle m_depthBindlessHandle{0};
};
} // namespace RenderPasses

#include "RenderPass.h"