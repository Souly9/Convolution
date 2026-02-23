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
#include "Core/Rendering/Passes/FrameResourceManager.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/ShadowMaps.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/View.h"
#include "Core/Rendering/Vulkan/VkAttachment.h"
#include "Core/SceneGraph/Mesh.h"
#include "Core/WindowManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "EASTL/fixed_vector.h"
#include "GBuffer.h"
#include "LightResourceManager.h"

class SharedResourceManager;

namespace RenderPasses
{
class ConvolutionRenderPass;

// RenderDataForPreProcessing logic transferred to FrameResourceManager



// A pass is responsible for rendering a view (aka, main pass renders the main
// camera view), can also execute solely on CPU side (think culling would be a
// pass too) Won't make it too complex but pass data will be roughly sorted
// based on the view type
struct MainPassData
{
    ::SharedResourceManager* pResourceManager{nullptr};
    GBuffer* pGbuffer{nullptr};
    RenderView mainView{};
    mathstl::Matrix mainCamViewMatrix{};
    mathstl::Matrix mainCamInvViewProj{};
    // Views we want to render with CSMs
    stltype::vector<CsmRenderView> csmViews;
    // Views we just render into normal shadowmaps whatever those will end up being
    stltype::vector<RenderView> shadowViews;
    stltype::vector<DescriptorSet*> viewDescriptorSets;
    stltype::hash_map<UBO::DescriptorContentsType, DescriptorSet*> bufferDescriptors;
    CascadedShadowMap directionalLightShadowMap{};
    Texture* pScreenSpaceShadowTexture{nullptr};
    BindlessTextureHandle screenSpaceShadows{0};
    BindlessTextureHandle depthBufferBindlessHandle{0};
    u32 cascades{0};
    f32 csmStepSize{0.0f};
};

enum class PassType
{
    EarlyAsyncCompute,
    PreProcess,
    DepthReliantCompute,
    Main,
    UI,
    Debug,
    Shadow,
    PostProcess,
    Composite
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

inline const stltype::fixed_vector<PassStage, 6> PASS_SCHEDULE = {
    PassStage{{PassType::EarlyAsyncCompute}},
    PassStage{{PassType::PreProcess}},
    PassStage{{PassType::DepthReliantCompute}},
    PassStage{{PassType::Main, PassType::Debug, PassType::Shadow}},
    PassStage{{PassType::UI}},
    PassStage{{PassType::Composite}},
};
inline const u32 STAGE_COUNT = PASS_SCHEDULE.size();

inline bool IsComputePass(PassType type)
{
    return type == PassType::EarlyAsyncCompute;
}

struct GraphicsFrameContext
{
    CommandPool cmdPool;
    stltype::fixed_vector<CommandBuffer*, SWAPCHAIN_IMAGES> cmdBuffers{SWAPCHAIN_IMAGES};
    stltype::fixed_vector<CommandBuffer*, SWAPCHAIN_IMAGES> compositeCmdBuffers{SWAPCHAIN_IMAGES};
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
    DepthBufferAttachmentVulkan depthAttachment;
    Texture* pScreenSpaceShadowTexture{nullptr};
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

    void SetSharedData(RenderView&& mainView, u32 frameIdx);

    void PreProcessDataForCurrentFrame(u32 frameIdx);

    void RegisterDebugCallbacks();

    // Delegates down to FrameResourceManager
    void UpdateSharedDataUBO(const void* data, size_t size, u32 frameIdx);
    void UpdateShadowViewUBO(const UBO::ShadowmapViewUBO& data, u32 frameIdx);
    void UpdateLightClusterSSBO(const UBO::LightClusterSSBO& data, u32 frameIdx);

    void DispatchSSBOTransfer(
        void* data, DescriptorSet* pDescriptor, u32 size, StorageBuffer* pSSBO, u32 offset = 0, u32 dstBinding = 0);
    void BlockUntilPassesFinished(u32 frameIdx);

    MainPassData& GetMainPassData(u32 idx) { return m_mainPassData[idx]; }
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

    // Helpers to split large Init / ExecutePasses
    void InitResourceManagerAndCallbacks();
    void CreatePassObjectsAndLayouts();
    void CreateUBOsAndMap();
    void CreateFrameRendererContexts();
    void InitPassesAndImGui();

    bool AnyPassWantsToRender() const;
    void PrepareMainPassDataForFrame(MainPassData& mainPassData, FrameRendererContext& ctx, u32 frameIdx);
    void RenderAllPassGroups(const MainPassData& mainPassData,
                             FrameRendererContext& ctx,
                             Semaphore& imageAvailableSemaphore);
    void RenderPassGroup(PassType groupType, const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer);
    void InitFrameContexts();

    // Inline layout transition helpers — record directly into pCmdBuffer
    void RecordInitialLayoutTransitions(CommandBuffer* pCmdBuffer,
                                        const stltype::vector<const Texture*>& allGbufferAndSwapchain);
    void RecordGBufferToShaderRead(CommandBuffer* pCmdBuffer,
                                   const stltype::vector<const Texture*>& gbufferTextures);
    void RecordUIToShaderRead(CommandBuffer* pCmdBuffer, const Texture* pUITexture);
    void RecordDepthToReadOnly(CommandBuffer* pCmdBuffer);
    void RecordSSSOutputToGeneral(CommandBuffer* pCmdBuffer);
    void RecordSSSOutputToShaderRead(CommandBuffer* pCmdBuffer);
    void RecordSwapchainToPresent(CommandBuffer* pCmdBuffer);

private:

    // GPU timing query
    GPUTimingQuery m_gpuTimingQuery;

    // Resource Manager
    ::SharedResourceManager m_resourceManager;
    FrameResourceManager m_frameResourceManager;

    // Only need one gbuffer
    GBuffer m_gbuffer;

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

    u32 m_currentSwapChainIdx;

    void CreateDepthAttachment();



    stltype::vector<u64> m_csmCascadeImGuiIDs{};

    u32 m_currentFrame{0};
    bool m_needsToPropagateMainDataUpdate{false};
    u32 m_frameIdxToPropagate{0};
    BindlessTextureHandle m_depthBindlessHandle{0};
    Texture* m_pScreenSpaceShadowTexture{nullptr};
    TextureHandle m_screenSpaceShadowsTextureHandle{0};
    BindlessTextureHandle m_screenSpaceShadowBindlessHandle{0};
};
} // namespace RenderPasses

#include "RenderPass.h"
