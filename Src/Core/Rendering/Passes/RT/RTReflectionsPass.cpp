#include "RTReflectionsPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include "Core/Rendering/Core/RT/RTSceneManager.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/TextureManager.h"

using namespace RenderPasses;

RTReflectionsPass::RTReflectionsPass() : RTComputePassBase("RTReflectionsPass")
{
    CreateSharedDescriptorLayout();
}

void RTReflectionsPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset(DescriptorPresets::Bindless(true));
    AppendLayoutPreset(DescriptorPresets::View());
    AppendLayoutPreset(DescriptorPresets::GlobalInstanceData());
    AppendLayoutPreset(DescriptorPresets::GBuffer());
    AppendLayoutPreset(DescriptorPresets::LightCluster());
    AppendLayoutPreset(DescriptorPresets::RTScene());
}

void RTReflectionsPass::Init(const SharedResourceManager& resourceManager)
{
    ScopedZone("RTReflectionsPass::Init");
    (void)resourceManager;
    BuildPipelines();
}

void RTReflectionsPass::BuildPipelines()
{
    auto computeShader = Shader("Shaders/RTReflections.comp.spv", "main");

    ShaderCollection shaders{};
    shaders.pComputeShader = &computeShader;

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst{};
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(RTReflectionsPushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    m_computePipeline = ComputePipeline(shaders, pipeInfo);
}

void RTReflectionsPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                            FrameRendererContext& previousFrameCtx,
                                            u32 thisFrameNum)
{
    (void)meshes;
    (void)previousFrameCtx;
    (void)thisFrameNum;
}

bool RTReflectionsPass::WantsToRender() const
{
    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    return mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTEnabled) &&
           mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTReflectionsEnabled);
}

#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"

void RTReflectionsPass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.DeclareContexts<
        PassCtx::BindlessWithImages,
        PassCtx::View,
        PassCtx::GlobalInstance,
        PassCtx::GBufferCtx,
        PassCtx::LightCluster,
        PassCtx::RTScene>();

    builder.ReadTexture(RGResourceID::MainDepth, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::GBufferAlbedo, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::GBufferNormal, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::GBufferUVMat, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::GBufferRoughness, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    auto reflections = builder.DeclareStorageTexture(RGResourceID::RTReflections, TexFormat::R16G16B16A16_FLOAT, RGSizeClass::RenderResolution);
    builder.WriteStorageImage(reflections, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_WRITE);
    builder.SetHasSideEffects();
}

#include "Core/Rendering/Core/RenderGraph/RGExecutionContext.h"

void RTReflectionsPass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("RTReflectionsPass::Render");

    CommandBuffer* pCmdBuffer = execCtx.pCmdBuffer;
    StartRenderPassProfilingScope(pCmdBuffer);

    const bool tlasReady = execCtx.HasReadyTLAS();
    const mathstl::Vector2 renderRes = execCtx.GetRenderResolution();

    static u32 s_logCounter = 0;
    if (s_logCounter++ % 120 == 0)
    {
        DEBUG_LOG_WARNF("[RTReflectionsPass] tlasReady: {}, renderRes: {:.0f}x{:.0f}, reflTexIdx: {}",
                        tlasReady ? 1 : 0, renderRes.x, renderRes.y,
                        execCtx.GetBindless(RGResourceID::RTReflections));
    }

    if (!tlasReady)
    {
        EndRenderPassProfilingScope(pCmdBuffer);
        return;
    }

    const auto& rtState = g_pApplicationState->GetCurrentApplicationState().renderState.rt;
    m_pushConstants.reflectionsTexIdx = execCtx.GetBindless(RGResourceID::RTReflections);
    m_pushConstants.debugMode = static_cast<u32>(rtState.reflectionsDebugMode);
    m_pushConstants.maxRayDistance = execCtx.GetZFar();
    m_pushConstants.reflectionIntensity = 1.0f;
    m_pushConstants.hasReadyTLAS = 1u;
    m_pushConstants.frameIndex = execCtx.GetFrameIndex();
    m_pushConstants.raysPerPixel = rtState.reflectionsRaysPerPixel;

    const u32 groupCountX = (static_cast<u32>(renderRes.x) + 7) / 8;
    const u32 groupCountY = (static_cast<u32>(renderRes.y) + 7) / 8;

    GenericComputeDispatchCmd dispatchCmd(&m_computePipeline, groupCountX, groupCountY, 1);
    dispatchCmd.descriptorSets = execCtx.GetDescriptors();
    dispatchCmd.SetPushConstants(0, m_pushConstants);
    pCmdBuffer->RecordCommand(dispatchCmd);

    EndRenderPassProfilingScope(pCmdBuffer);
}
