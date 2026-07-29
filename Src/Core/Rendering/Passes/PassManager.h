#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/Attachment.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/Defines/LightDefines.h"
#include "Core/Rendering/Core/GPUTimingQuery.h"
#include "Core/Events/EventSystem.h"
#include "Core/Rendering/Core/FrameTransitionRecorder.h"
#include "Core/Rendering/Core/FrameResourceManager.h"
#include "Core/Rendering/Core/RenderTargetManager.h"
#include "Core/Rendering/Core/RenderTextureImGuiRegistry.h"
#include "Core/Rendering/Core/RT/RTResourceManager.h"
#include "Core/Rendering/Core/RT/RTSceneManager.h"
#include "Core/Rendering/Core/ShadowMaps.h"
#include "Core/Rendering/Core/ShadowMapManager.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/RenderGraph/RenderGraph.h"
#include "Core/Rendering/Core/View.h"
#include "Core/Rendering/Core/Buffer.h"
#include <SimpleMath/SimpleMath.h>
#include "EASTL/fixed_vector.h"
#include "MainPassData.h"

class SharedResourceManager;

namespace RenderPasses
{
class ConvolutionRenderPass;

enum class PassType
{
    LightTransformCompute,
    ClusterGenCompute,
    TileAssignmentCompute,
    EarlyAsyncCompute,
    PreProcess,
    DepthReliantCompute,
    Main,
    Lighting,    // Full screen lighting pass
    TAA,         // Temporal Anti-Aliasing
    SMAA,        // Subpixel Morphological Anti-Aliasing
    DLSS,        // Deep Learning Super Sampling
    DLSS_RR,     // Deep Learning Super Sampling Ray Reconstruction
    XeSS,        // Intel XeSS
    Composite,
    UI,
    Debug,
    Shadow,
    PostProcess, // General post process
    RTAOCompute,
    RTReflectionsCompute,
    RTComposite,
};

// ============================================================================
// PASS SCHEDULE
// Defines stages of execution. Groups within the same stage run in parallel
// on the GPU (each submits independently, waiting on the same timeline value).
// Stages are sequentially ordered — each stage waits for the previous to finish.
// Edit PASS_SCHEDULE to control which groups are parallel.
// ============================================================================
struct PassStage
{
    stltype::fixed_vector<PassType, 8> groups;
};

inline const stltype::fixed_vector<PassStage, 11> PASS_SCHEDULE = {
    PassStage{{PassType::EarlyAsyncCompute}},
    PassStage{{PassType::PreProcess}},
    PassStage{{PassType::DepthReliantCompute}},
    PassStage{{PassType::Main, PassType::Debug, PassType::Shadow}},
    PassStage{{PassType::Lighting}},
    PassStage{{PassType::RTAOCompute, PassType::RTReflectionsCompute}},
    PassStage{{PassType::RTComposite}},
    PassStage{{PassType::TAA, PassType::DLSS, PassType::DLSS_RR, PassType::XeSS}},
    PassStage{{PassType::Composite}},
    PassStage{{PassType::SMAA}},
    PassStage{{PassType::UI}},
};
inline const u32 STAGE_COUNT = PASS_SCHEDULE.size();

inline bool IsComputePass(PassType type)
{
    return type == PassType::EarlyAsyncCompute || 
           type == PassType::LightTransformCompute || 
           type == PassType::ClusterGenCompute ||
           type == PassType::TileAssignmentCompute ||
           type == PassType::PostProcess ||
           type == PassType::TAA ||
           type == PassType::DLSS ||
           type == PassType::DLSS_RR ||
           type == PassType::XeSS ||
           type == PassType::RTAOCompute ||
           type == PassType::RTReflectionsCompute ||
           type == PassType::RTComposite;
}

struct GraphicsFrameContext
{
    CommandPool cmdPool;
    stltype::fixed_vector<CommandBuffer*, SWAPCHAIN_IMAGES> cmdBuffers{SWAPCHAIN_IMAGES};
    stltype::fixed_vector<CommandBuffer*, SWAPCHAIN_IMAGES> lightingCmdBuffers{SWAPCHAIN_IMAGES};
    stltype::fixed_vector<CommandBuffer*, SWAPCHAIN_IMAGES> compositeCmdBuffers{SWAPCHAIN_IMAGES};
    stltype::fixed_vector<CommandBuffer*, SWAPCHAIN_IMAGES> presentTransitionCmdBuffers{SWAPCHAIN_IMAGES};
    stltype::fixed_vector<CommandBuffer*, SWAPCHAIN_IMAGES> depthPrePassCmdBuffers{SWAPCHAIN_IMAGES};
    stltype::vector<CommandBuffer*> batchCmdBuffers[SWAPCHAIN_IMAGES];
    bool initialized{false};
};

struct ComputeFrameContext
{
    CommandPool cmdPool;
    stltype::fixed_vector<CommandBuffer*, SWAPCHAIN_IMAGES> cmdBuffers{SWAPCHAIN_IMAGES};
    stltype::fixed_vector<CommandBuffer*, SWAPCHAIN_IMAGES> sssComputeCmdBuffers{SWAPCHAIN_IMAGES};
    stltype::fixed_vector<CommandBuffer*, SWAPCHAIN_IMAGES> rtComputeCmdBuffers{SWAPCHAIN_IMAGES};
    stltype::vector<CommandBuffer*> batchCmdBuffers[SWAPCHAIN_IMAGES];
    bool initialized{false};
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
        m_mainPassData.resize(SWAPCHAIN_IMAGES);
        m_imageAvailableSemaphores.resize(SWAPCHAIN_IMAGES);
        m_imageAvailableFences.resize(SWAPCHAIN_IMAGES);
        m_renderFinishedFences.resize(SWAPCHAIN_IMAGES);
        g_pEventSystem->AddBaseInitEventCallback([&](const auto&) { Init(); });
    }
    ~PassManager();

    void Init();
    bool NeedsResizeDependentResourceRecreate(const mathstl::Vector2& swapchainResolution) const;
    void RecreateResizeDependentResources(const mathstl::Vector2& swapchainResolution, bool swapchainRecreated);
    void AddPass(PassType type, stltype::unique_ptr<ConvolutionRenderPass>&& pass);
    void TransferPassData(const PassGeometryData& passData, u32 frameIdx);

    void ExecutePasses(u32 frameIdx);
    void ReadAndPublishTimingResults(u32 frameIdx);

    // Can be called from many different threads
    void SetEntityMeshDataForFrame(EntityMeshDataMap&& data, u32 frameIdx);
    void SetEntityTransformDataForFrame(TransformSystemData&& data, u32 frameIdx);
    void SetLightDataForFrame(PointLightVector&& data, DirLightVector&& dirLights, u32 frameIdx);
    void SetLightDeltaForFrame(stltype::vector<LightDeltaUpdate>&& updates, bool dirLightDirty,
                               const DirectionalRenderLight& dirLight, u32 frameIdx);

    void SetSharedData(RenderView&& mainView, u32 frameIdx);
    void PreProcessDataForCurrentFrame(u32 frameIdx, u64 jitterFrameNumber);
    void ResetSceneState();

    void RegisterDebugCallbacks();

    // Delegates down to FrameResourceManager
    void UpdateSharedDataUBO(const void* data, size_t size, u32 frameIdx);
    void UpdateLightClusterSSBO(const UBO::LightClusterSSBO& data, u32 frameIdx);

    void DispatchSSBOTransfer(
        void* data, DescriptorSet::Ptr pDescriptor, u32 size, StorageBuffer* pSSBO, u32 offset = 0, u32 dstBinding = 0);
    bool BlockUntilPassesFinished(u32 frameIdx);

    MainPassData& GetMainPassData(u32 idx) { return m_mainPassData[idx]; }
    const MainPassData::PassManagerRenderState& GetRenderState() const { return m_renderState; }
    void SetRenderJitter(const mathstl::Vector2& jitter)
    {
        m_renderState.previousJitter = m_renderState.jitter;
        m_renderState.jitter = jitter;
    }
    ::SharedResourceManager& GetResourceManager() { return m_resourceManager; }
    void RecreateShadowMapsPublic(u32 cascades, const mathstl::Vector2& extents) { RecreateShadowMaps(cascades, extents); }
    void RegisterImGuiTexturesPublic() { m_imguiRegistry.RegisterShadowMapTextures(m_shadowMapManager.GetShadowMap()); }
    void PreProcessMeshDataPublic(const stltype::vector<PassMeshData>& meshes, u32 lastFrame, u32 curFrame) { PreProcessMeshData(meshes, lastFrame, curFrame); }
    void TransferPassDataPublic(PassGeometryData&& passData, u32 frameIdx) { TransferPassData(std::move(passData), frameIdx); }


    // Mainly used to hot reload shaders
    // Rebuilding all pipelines for all passes is not the most efficient but we
    // don't have that many and won't be doing it often anyway
    void RebuildPipelinesForAllPasses();

    // GPU timing query accessors
    const stltype::vector<PassTimingResult>& GetPassTimingResults() const;
    f32 GetTotalGPUTimeMs() const;

    static inline stltype::atomic<u64> s_globalTimelineCounter{1};

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
    void UpdateTemporalResources(MainPassData& mainPassData);
    void PrepareMainPassDataForFrame(MainPassData& mainPassData, FrameRendererContext& ctx, u32 frameIdx);
    void RenderAllPassGroups(const MainPassData& mainPassData,
                             FrameRendererContext& ctx,
                             Semaphore& imageAvailableSemaphore);
    void InitFrameContexts();
    void UpdateGBufferUBO(const MainPassData& data);

    CommandBuffer* GetGraphicsCommandBuffer(u32 frameIdx, u32 batchIdx);
    CommandBuffer* GetComputeCommandBuffer(u32 frameIdx, u32 batchIdx);

private:

    // GPU timing query
    GPUTimingQuery m_gpuTimingQuery;

    // Resource Manager
    ::SharedResourceManager m_resourceManager;
    RT::RTSceneManager m_rtSceneManager;
    RT::RTResourceManager m_rtResourceManager;
    FrameResourceManager m_frameResourceManager;
    RenderTargetManager m_renderTargetManager;
    ShadowMapManager m_shadowMapManager;
    RenderTextureImGuiRegistry m_imguiRegistry;
    FrameTransitionRecorder m_transitionRecorder;
    RenderGraph m_renderGraph;

    // Pass data for each frame
    stltype::hash_map<PassType, stltype::vector<stltype::unique_ptr<ConvolutionRenderPass>>> m_passes{};
    stltype::fixed_vector<MainPassData, SWAPCHAIN_IMAGES> m_mainPassData{};
    stltype::fixed_vector<Semaphore, SWAPCHAIN_IMAGES> m_imageAvailableSemaphores{};
    stltype::fixed_vector<Fence, SWAPCHAIN_IMAGES> m_imageAvailableFences{};
    stltype::fixed_vector<Fence, SWAPCHAIN_IMAGES> m_renderFinishedFences{};
    GraphicsFrameContext m_graphicsFrameCtx;
    ComputeFrameContext m_computeFrameCtx;

    u32 m_currentSwapChainIdx{0};

    u32 m_currentFrame{0};
    bool m_needsToPropagateMainDataUpdate{false};
    u32 m_frameIdxToPropagate{0};
    bool m_passesInitialized{false};
    u32 m_instanceBufferUpdateTimingIndex;
    u32 m_clearTileCountersTimingIndex{UINT32_MAX};
    MainPassData::PassManagerRenderState m_renderState{};
};
} // namespace RenderPasses

#include "RenderPass.h"
