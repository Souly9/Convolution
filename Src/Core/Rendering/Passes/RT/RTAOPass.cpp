#include "RTAOPass.h"
#include "Core/Global/FrameGlobals.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/TextureManager.h"

using namespace RenderPasses;

RTAOPass::RTAOPass() : RTComputePassBase("RTAOPass")
{
    CreateSharedDescriptorLayout();
}

void RTAOPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset(DescriptorPresets::Bindless(true));
    AppendLayoutPreset(DescriptorPresets::View());
    AppendLayoutPreset(DescriptorPresets::GlobalInstanceData());
    AppendLayoutPreset(DescriptorPresets::GBuffer());
    AppendLayoutPreset(DescriptorPresets::LightCluster());
    AppendLayoutPreset(DescriptorPresets::RTScene());
}

void RTAOPass::Init(const SharedResourceManager& resourceManager)
{
    ScopedZone("RTAOPass::Init");
    (void)resourceManager;
    BuildPipelines();
}

void RTAOPass::BuildPipelines()
{
    auto computeShader = Shader("Shaders/RTAO.comp.spv", "main");

    ShaderCollection shaders{};
    shaders.pComputeShader = &computeShader;

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst{};
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(RTAOPushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    m_computePipeline = ComputePipeline(shaders, pipeInfo);
}

void RTAOPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                   FrameRendererContext& previousFrameCtx,
                                   u32 thisFrameNum)
{
    (void)meshes;
    (void)previousFrameCtx;
    (void)thisFrameNum;
}

bool RTAOPass::WantsToRender() const
{
    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    return mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTEnabled) &&
           mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTAOEnabled);
}

#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"

void RTAOPass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.DeclareContexts<
        PassCtx::BindlessWithImages,
        PassCtx::View,
        PassCtx::GlobalInstance,
        PassCtx::GBufferCtx,
        PassCtx::LightCluster,
        PassCtx::RTScene>();

    builder.ReadTexture(RGResourceID::MainDepth, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::GBufferNormal, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    auto rtao = builder.DeclareStorageTexture(RGResourceID::RTAOOutput, TexFormat::R8_UNORM, RGSizeClass::RenderResolution);
    builder.WriteStorageImage(rtao, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_WRITE);
    builder.SetHasSideEffects();
}

#include "Core/Rendering/Core/RenderGraph/RGExecutionContext.h"

void RTAOPass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("RTAOPass::Render");

    CommandBuffer* pCmdBuffer = execCtx.pCmdBuffer;
    StartRenderPassProfilingScope(pCmdBuffer);

    const bool hasReadyTLAS = execCtx.HasReadyTLAS();

    const auto& rtState = g_pApplicationState->GetCurrentApplicationState().renderState.rt;
    m_pushConstants.rtaoTexIdx = execCtx.GetBindless(RGResourceID::RTAOOutput);
    m_pushConstants.hasReadyTLAS = hasReadyTLAS ? 1u : 0u;
    m_pushConstants.frameIndex = execCtx.GetFrameIndex();
    m_pushConstants.raysPerPixel = rtState.aoRaysPerPixel;
    m_pushConstants.aoRadius = rtState.aoRadius;
    m_pushConstants.aoIntensity = rtState.aoIntensity;

    const u32 groupCountX = (static_cast<u32>(execCtx.GetRenderResolution().x) + 7) / 8;
    const u32 groupCountY = (static_cast<u32>(execCtx.GetRenderResolution().y) + 7) / 8;

    GenericComputeDispatchCmd dispatchCmd(&m_computePipeline, groupCountX, groupCountY, 1);
    dispatchCmd.descriptorSets = execCtx.GetDescriptors();
    dispatchCmd.SetPushConstants(0, m_pushConstants);
    pCmdBuffer->RecordCommand(dispatchCmd);

    EndRenderPassProfilingScope(pCmdBuffer);
}
