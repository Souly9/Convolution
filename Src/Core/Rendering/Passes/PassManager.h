#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/Attachment.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/Defines/LightDefines.h"
#include "Core/Rendering/Core/GPUTimingQuery.h"
#include "Core/Events/EventSystem.h"
#include "Core/Rendering/Core/FrameResourceManager.h"
#include "Core/Rendering/Core/RT/RTResourceManager.h"
#include "Core/Rendering/Core/RT/RTSceneManager.h"
#include "Core/Rendering/Core/ShadowMaps.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/View.h"
#include "Core/Rendering/Core/Buffer.h"
#include <SimpleMath/SimpleMath.h>
#include "EASTL/fixed_vector.h"
#include "GBuffer.h"
class SharedResourceManager;

namespace RenderPasses
{
class ConvolutionRenderPass;

// A pass is responsible for rendering a view (aka, main pass renders the main
// camera view), can also execute solely on CPU side (think culling would be a
// pass too) Won't make it too complex but pass data will be roughly sorted
// based on the view type
struct MainPassData
{
    struct TemporalResources
    {
        Texture* pCurrentColorTexture{nullptr};
        Texture* pTemporalCurrentColorTexture{nullptr};
        Texture* pHistoryColorTexture{nullptr};
        Texture* pResolveTexture{nullptr};
        Texture* pPostAAColorTexture{nullptr};
        Texture* pCurrentDepthTexture{nullptr};
        Texture* pHistoryDepthTexture{nullptr};
        BindlessTextureHandle currentColorHandle{0};
        BindlessTextureHandle temporalCurrentColorHandle{0};
        BindlessTextureHandle historyColorHandle{0};
        BindlessTextureHandle resolveHandle{0};
        BindlessTextureHandle postAAColorHandle{0};
        BindlessTextureHandle currentDepthHandle{0};
        BindlessTextureHandle historyDepthHandle{0};
    };

    struct PassManagerRenderState
    {
        bool recreatedThisFrame{false};
        mathstl::Vector2 renderResolution{};
        mathstl::Vector2 swapchainResolution{};
        mathstl::Vector2 jitter{};
        mathstl::Vector2 previousJitter{};
    };

    ::SharedResourceManager* pResourceManager{nullptr};
    GBuffer* pGbuffer{nullptr};
    Texture* pMainDepthTexture{nullptr};
    Texture* pLastFrameDepthTexture{nullptr};
    TemporalResources temporalResources{};
    PassManagerRenderState renderState{};
    RenderView mainView{};
    mathstl::Matrix mainCamViewMatrix{};
    mathstl::Matrix mainCamInvViewProj{};
    // Views we want to render with CSMs
    stltype::vector<CsmRenderView> csmViews;
    // Views we just render into normal shadowmaps whatever those will end up being
    stltype::vector<RenderView> shadowViews;
    stltype::vector<DescriptorSet::Ptr> viewDescriptorSets;
    stltype::hash_map<UBO::DescriptorContentsType, DescriptorSet::Ptr> bufferDescriptors;
    CascadedShadowMap directionalLightShadowMap{};
    Texture* pScreenSpaceShadowTexture{nullptr};
    BindlessTextureHandle screenSpaceShadows{0};
    BindlessTextureHandle depthBufferBindlessHandle{0};
    RT::RTSceneManager* pRTSceneManager{nullptr};
    Texture* pRTDebugViewTexture{nullptr};
    Texture* pRTReflectionsTexture{nullptr};
    Texture* pRTReflectedSceneColorTexture{nullptr};
    BindlessTextureHandle rtDebugTextureHandle{0};
    BindlessTextureHandle rtReflectionsTextureHandle{0};
    BindlessTextureHandle rtReflectedSceneColorTextureHandle{0};
    u32 cascades{0};
    f32 csmStepSize{0.0f};

    // SMAA Intermediates
    Texture* pSMAAEdgesTexture{nullptr};
    Texture* pSMAABlendTexture{nullptr};
    BindlessTextureHandle smaaEdges{0};
    BindlessTextureHandle smaaBlend{0};
};

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
    TemporalTonemap,
    Composite,
    UI,
    Debug,
    Shadow,
    PostProcess, // General post process
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
    PassStage{{PassType::PostProcess}},
    PassStage{{PassType::TemporalTonemap}},
    PassStage{{PassType::TAA, PassType::DLSS}},
    PassStage{{PassType::SMAA}},
    PassStage{{PassType::Composite}},
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
           type == PassType::TemporalTonemap ||
           type == PassType::TAA ||
           type == PassType::DLSS;
}

struct GraphicsFrameContext
{
    CommandPool cmdPool;
    stltype::fixed_vector<CommandBuffer*, SWAPCHAIN_IMAGES> cmdBuffers{SWAPCHAIN_IMAGES};
    stltype::fixed_vector<CommandBuffer*, SWAPCHAIN_IMAGES> compositeCmdBuffers{SWAPCHAIN_IMAGES};
    stltype::fixed_vector<CommandBuffer*, SWAPCHAIN_IMAGES> presentTransitionCmdBuffers{SWAPCHAIN_IMAGES};
    stltype::fixed_vector<CommandBuffer*, SWAPCHAIN_IMAGES> depthPrePassCmdBuffers{SWAPCHAIN_IMAGES};
    bool initialized{false};
};

struct ComputeFrameContext
{
    CommandPool cmdPool;
    stltype::fixed_vector<CommandBuffer*, SWAPCHAIN_IMAGES> cmdBuffers{SWAPCHAIN_IMAGES};
    stltype::fixed_vector<CommandBuffer*, SWAPCHAIN_IMAGES> sssComputeCmdBuffers{SWAPCHAIN_IMAGES};
    bool initialized{false};
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
    DepthAttachment depthAttachment;
    Texture* pScreenSpaceShadowTexture{nullptr};
    Texture* pSMAAEdgesTexture{nullptr};
    Texture* pSMAABlendTexture{nullptr};
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
    void RecreateGbuffers(const mathstl::Vector2& renderResolution, const mathstl::Vector2& outputResolution);
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

    void RegisterDebugCallbacks();

    // Delegates down to FrameResourceManager
    void UpdateSharedDataUBO(const void* data, size_t size, u32 frameIdx);
    void UpdateShadowViewUBO(const UBO::ShadowmapViewUBO& data, u32 frameIdx);
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
    void RegisterImGuiTexturesPublic() { RegisterImGuiTextures(); }
    void PreProcessMeshDataPublic(const stltype::vector<PassMeshData>& meshes, u32 lastFrame, u32 curFrame) { PreProcessMeshData(meshes, lastFrame, curFrame); }
    void TransferPassDataPublic(PassGeometryData&& passData, u32 frameIdx) { TransferPassData(std::move(passData), frameIdx); }


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
    void RegisterImGuiTextures();
    void UpdateImGuiGbufferTextures();

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
    void RenderPassGroup(PassType groupType, const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer);
    void InitFrameContexts();
    void UpdateGBufferUBO(const MainPassData& data);

    // Inline layout transition helpers — record directly into pCmdBuffer
    void RecordInitialLayoutTransitions(CommandBuffer* pCmdBuffer,
                                        const stltype::fixed_vector<const Texture*, 16>& allGbufferAndSwapchain);
    void RecordPendingTextureUploadTransitions(CommandBuffer* pCmdBuffer);
    void RecordGBufferToShaderRead(CommandBuffer* pCmdBuffer,
                                   const stltype::fixed_vector<const Texture*, 8>& gbufferTextures);
    void RecordVelocityClear(CommandBuffer* pCmdBuffer);
    void RecordUIToShaderRead(CommandBuffer* pCmdBuffer, const Texture* pUITexture);
    void RecordDepthToReadOnly(CommandBuffer* pCmdBuffer);
    void RecordThisFrameColorToRead(CommandBuffer* pCmdBuffer);
    void RecordTemporalCurrentColorToGeneral(CommandBuffer* pCmdBuffer);
    void RecordTemporalCurrentColorToRead(CommandBuffer* pCmdBuffer);
    void RecordResolveToGeneral(CommandBuffer* pCmdBuffer);
    void RecordResolveToRead(CommandBuffer* pCmdBuffer);
    void RecordTemporalColorTargetsToRead(CommandBuffer* pCmdBuffer);
    void RecordCopyTextureToResolve(CommandBuffer* pCmdBuffer, Texture* pSourceTexture);
    void RecordClearColorTexture(CommandBuffer* pCmdBuffer,
                                 Texture* pTexture,
                                 ImageLayout oldLayout,
                                 ImageLayout finalLayout);
    void RecordSSSOutputToGeneral(CommandBuffer* pCmdBuffer);
    void RecordSSSOutputToShaderRead(CommandBuffer* pCmdBuffer);
    void RecordDLSSExposureUpdate(CommandBuffer* pCmdBuffer);
    void RecordSwapchainToPresent(CommandBuffer* pCmdBuffer);

private:

    // GPU timing query
    GPUTimingQuery m_gpuTimingQuery;

    // Resource Manager
    ::SharedResourceManager m_resourceManager;
    RT::RTSceneManager m_rtSceneManager;
    RT::RTResourceManager m_rtResourceManager;
    FrameResourceManager m_frameResourceManager;

    // Only need one gbuffer
    GBuffer m_gbuffer;
    u32 m_temporalResolveWrites{0};
    bool m_temporalCurrentColorWritten{false};

    // Pass data for each frame
    stltype::hash_map<PassType, stltype::vector<stltype::unique_ptr<ConvolutionRenderPass>>> m_passes{};
    stltype::fixed_vector<MainPassData, SWAPCHAIN_IMAGES> m_mainPassData{SWAPCHAIN_IMAGES};
    stltype::fixed_vector<Semaphore, SWAPCHAIN_IMAGES> m_imageAvailableSemaphores{SWAPCHAIN_IMAGES};
    stltype::fixed_vector<Fence, SWAPCHAIN_IMAGES> m_imageAvailableFences{SWAPCHAIN_IMAGES};
    // Fences to track when rendering to each swapchain image completes (for semaphore reuse safety)
    stltype::fixed_vector<Fence, SWAPCHAIN_IMAGES> m_renderFinishedFences{SWAPCHAIN_IMAGES};
    GraphicsFrameContext m_graphicsFrameCtx;
    ComputeFrameContext m_computeFrameCtx;

    // Global attachment infos for all passes like gbuffer, depth buffer or
    // swapchain textures
    RendererAttachmentInfo m_globalRendererAttachments;

    // Depth texture created during Init and used later for attachments/ImGui
    Texture* m_pDepthTexture{nullptr};
    Texture* m_pLastFrameDepthTexture{nullptr};
    TextureHandle m_depthTextureHandle{0};
    TextureHandle m_lastFrameDepthTextureHandle{0};

    u32 m_currentSwapChainIdx{0};

    void CreateDepthAttachment();



    stltype::vector<u64> m_csmCascadeImGuiIDs{};
    stltype::vector<u64> m_gbufferImGuiIDs{};

    u32 m_currentFrame{0};
    bool m_needsToPropagateMainDataUpdate{false};
    u32 m_frameIdxToPropagate{0};
    BindlessTextureHandle m_depthBindlessHandle{0};
    BindlessTextureHandle m_lastFrameDepthBindlessHandle{0};
    Texture* m_pScreenSpaceShadowTexture{nullptr};
    TextureHandle m_screenSpaceShadowsTextureHandle{0};
    BindlessTextureHandle m_screenSpaceShadowBindlessHandle{0};

    Texture* m_pSMAAEdgesTexture{nullptr};
    TextureHandle m_smaaEdgesTextureHandle{0};
    BindlessTextureHandle m_smaaEdgesBindlessHandle{0};

    Texture* m_pSMAABlendTexture{nullptr};
    TextureHandle m_smaaBlendTextureHandle{0};
    BindlessTextureHandle m_smaaBlendBindlessHandle{0};

    Texture* m_pDLSSExposureTexture{nullptr};
    TextureHandle m_dlssExposureTextureHandle{0};
    StagingBuffer m_dlssExposureStagingBuffer{};
    bool m_dlssExposureTextureInitialized{false};
    bool m_passesInitialized{false};
    u32 m_instanceBufferUpdateTimingIndex;
    u32 m_clearTileCountersTimingIndex{UINT32_MAX};
    MainPassData::PassManagerRenderState m_renderState{};
    static inline stltype::atomic<u64> s_globalTimelineCounter{1};
};
} // namespace RenderPasses

#include "RenderPass.h"
