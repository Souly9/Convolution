#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/AntiAliasing.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/Defines/LightDefines.h"
#include "Core/Rendering/Core/GPUTimingQuery.h"
#include "Core/Events/EventSystem.h"
#include "Core/Rendering/Core/FrameResourceManager.h"
#include "Core/Rendering/Core/RenderTextureImGuiRegistry.h"

#include "Core/Rendering/Core/RT/RTSceneManager.h"
#include "Core/Rendering/Core/ShadowMaps.h"
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
class DebugShapePass;

// One command buffer per render-graph batch of a queue, per frame slot
struct QueueFrameContext
{
    CommandPool cmdPool;
    stltype::vector<CommandBuffer*> batchCmdBuffers[SWAPCHAIN_IMAGES];
};

class PassManager
{
public:
    PassManager()
    {
        m_mainPassData.resize(SWAPCHAIN_IMAGES);
        m_imageAvailableSemaphores.resize(SWAPCHAIN_IMAGES);
        m_imageAvailableFences.resize(SWAPCHAIN_IMAGES);
        g_engine.GetEventSystem().AddBaseInitEventCallback([&](const auto&) { Init(); });
    }
    ~PassManager();

    void Init();
    bool NeedsResizeDependentResourceRecreate(const mathstl::Vector2& swapchainResolution) const;
    void RecreateResizeDependentResources(const mathstl::Vector2& swapchainResolution);
    void AddPass(stltype::unique_ptr<ConvolutionRenderPass>&& pass);

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

    bool BlockUntilPassesFinished(u32 frameIdx);

    MainPassData& GetMainPassData(u32 idx) { return m_mainPassData[idx]; }
    const MainPassData::PassManagerRenderState& GetRenderState() const { return m_renderState; }
    ::SharedResourceManager& GetResourceManager() { return m_resourceManager; }
    RenderGraph& GetRenderGraph() { return m_renderGraph; }
    void PreProcessMeshDataPublic(const stltype::vector<PassMeshData>& meshes) { PreProcessMeshData(meshes); }
    void RecreateShadowMapsPublic(u32 cascades, const mathstl::Vector2& extents) { RecreateShadowMaps(cascades, extents); }


    // Mainly used to hot reload shaders
    // Rebuilding all pipelines for all passes is not the most efficient but we
    // don't have that many and won't be doing it often anyway
    void RebuildPipelinesForAllPasses();

    static inline stltype::atomic<u64> s_globalTimelineCounter{1};

protected:
    void PreProcessMeshData(const stltype::vector<PassMeshData>& meshes);
    void RebuildMeshDataForSlot(u32 frameIdx, FrameRendererContext& ctx);
    // Back to the primitives only, they stay resident because fullscreen passes draw them from the scene buffers
    void ResetSceneGeometry();

    void RecreateShadowMaps(u32 cascades, const mathstl::Vector2& extents);
    // Helpers to split large Init / ExecutePasses
    void InitResourceManagerAndCallbacks();
    void CreateUBOsAndMap();

    void InitPassesAndImGui();

    void PrepareMainPassDataForFrame(MainPassData& mainPassData, FrameRendererContext& ctx, u32 frameIdx);
    void SetupRenderGraph(const MainPassData& mainPassData, FrameRendererContext& ctx);
    void CompileAndExecuteRenderGraph(const MainPassData& mainPassData,
                                      FrameRendererContext& ctx,
                                      Semaphore& imageAvailableSemaphore);
    void InitFrameContexts();
    void UpdateAAFrameConfig(u32 frameIdx);
    void UpdateGBufferUBO(u32 frameIdx);

    CommandBuffer* GetBatchCommandBuffer(QueueType queueType, u32 frameIdx, u32 batchIdx);

private:

    // GPU timing query
    GPUTimingQuery m_gpuTimingQuery;

    // Resource Manager
    ::SharedResourceManager m_resourceManager;
    RT::RTSceneManager m_rtSceneManager;

    FrameResourceManager m_frameResourceManager;
    RenderTextureImGuiRegistry m_imguiRegistry;
    RenderGraph m_renderGraph;

    // Pass data for each frame
    stltype::vector<stltype::unique_ptr<ConvolutionRenderPass>> m_passes{};
    // Owned by m_passes; the GBuffer UBO tells the composite whether its overlay was drawn
    DebugShapePass* m_pDebugShapePass{nullptr};
    stltype::fixed_vector<MainPassData, SWAPCHAIN_IMAGES> m_mainPassData{};
    stltype::fixed_vector<Semaphore, SWAPCHAIN_IMAGES> m_imageAvailableSemaphores{};
    stltype::fixed_vector<Fence, SWAPCHAIN_IMAGES> m_imageAvailableFences{};
    QueueFrameContext m_graphicsFrameCtx;
    QueueFrameContext m_computeFrameCtx;

    u32 m_currentSwapChainIdx{0};
    stltype::vector<PassMeshData> m_pendingMeshData;
    u32 m_meshRebuildSlotMask{0};

    bool m_passesInitialized{false};
    MainPassData::PassManagerRenderState m_renderState{};
    AA::Temporal m_lastTemporal{AA::Temporal::None};
    u32 m_lastTemporalResetGeneration{0};
};
} // namespace RenderPasses

#include "RenderPass.h"
