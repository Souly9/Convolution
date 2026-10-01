#include "RTDebugViewPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include "Core/Rendering/Core/RT/RTSceneManager.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/TextureManager.h"

using namespace RenderPasses;

RTDebugViewPass::RTDebugViewPass() : RTComputePassBase("RTDebugViewPass")
{
    CreateSharedDescriptorLayout();
}

void RTDebugViewPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset(DescriptorPresets::Bindless(true));
    AppendLayoutPreset(DescriptorPresets::View());
    AppendLayoutPreset(DescriptorPresets::GlobalInstanceData());
    AppendLayoutPreset(DescriptorPresets::GBuffer());
    AppendLayoutPreset(DescriptorPresets::LightCluster());
    AppendLayoutPreset(DescriptorPresets::RTScene(true));
}

void RTDebugViewPass::Init(const SharedResourceManager& resourceManager)
{
    ScopedZone("RTDebugViewPass::Init");
    (void)resourceManager;
    BuildPipelines();
}

void RTDebugViewPass::BuildPipelines()
{
    auto computeShader = Shader("Shaders/RTDebugView.comp.spv", "main");

    ShaderCollection shaders{};
    shaders.pComputeShader = &computeShader;

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst{};
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(RTDebugViewPushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    m_computePipeline = ComputePipeline(shaders, pipeInfo);
}

void RTDebugViewPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                          FrameRendererContext& previousFrameCtx,
                                          u32 thisFrameNum)
{
    (void)meshes;
    (void)previousFrameCtx;
    (void)thisFrameNum;
}

bool RTDebugViewPass::WantsToRender() const
{
    const auto& renderState = g_engine.GetApplicationState().GetCurrentApplicationState().renderState;
    return mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTEnabled) &&
           mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTDebugEnabled);
}

#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"

void RTDebugViewPass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
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

    auto debugTex = builder.DeclareStorageTexture(RGResourceID::GBufferDebug, TexFormat::R16G16B16A16_FLOAT, RGSizeClass::RenderResolution);
    builder.WriteStorageImage(debugTex, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_WRITE);
    builder.SetHasSideEffects();
}

#include "Core/Rendering/Core/RenderGraph/RGExecutionContext.h"

void RTDebugViewPass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("RTDebugViewPass::Render");

    CommandBuffer* pCmdBuffer = execCtx.pCmdBuffer;
    StartRenderPassProfilingScope(pCmdBuffer);

    if (!execCtx.HasReadyTLAS())
    {
        EndRenderPassProfilingScope(pCmdBuffer);
        return;
    }

    const auto& rtState = g_engine.GetApplicationState().GetCurrentApplicationState().renderState.rt;
    m_pushConstants.outputTexIdx = execCtx.GetBindless(RGResourceID::GBufferDebug);
    m_pushConstants.debugMode = rtState.debugMode;
    m_pushConstants.maxRayDistance = execCtx.GetZFar();

    const u32 groupCountX = (static_cast<u32>(execCtx.GetRenderResolution().x) + 7) / 8;
    const u32 groupCountY = (static_cast<u32>(execCtx.GetRenderResolution().y) + 7) / 8;

    GenericComputeDispatchCmd dispatchCmd(&m_computePipeline, groupCountX, groupCountY, 1);
    dispatchCmd.descriptorSets = execCtx.GetDescriptors();
    dispatchCmd.SetPushConstants(0, m_pushConstants);
    pCmdBuffer->RecordCommand(dispatchCmd);
    EndRenderPassProfilingScope(pCmdBuffer);
}
