#include "TileAssignmentComputePass.h"
#include "../PassManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/RenderGraph/PassContext.h"
#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"

#define ViewSet             0
#define LightClusterSet     1
#define ClusterGridSet      2
#define ViewSpaceLightsSet  3

using namespace RenderPasses;

TileAssignmentComputePass::TileAssignmentComputePass() : ConvolutionRenderPass("TileAssignmentComputePass")
{
    CreateSharedDescriptorLayout();
}

void TileAssignmentComputePass::Init(RendererAttachmentInfo& attachmentInfo,
                                     const SharedResourceManager& resourceManager)
{
    ScopedZone("TileAssignmentComputePass::Init");
    BuildBuffers();
    BuildPipelines();
}

void TileAssignmentComputePass::BuildBuffers()
{
}

void TileAssignmentComputePass::BuildPipelines()
{
    ScopedZone("TileAssignmentComputePass::BuildPipelines");

    auto shader = Shader("Shaders/TileLightAssignment.comp.spv", "main");

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst;
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(ClusterPushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    ShaderCollection shaders{};
    shaders.pComputeShader = &shader;
    m_pipeline = ComputePipeline(shaders, pipeInfo);
}

void TileAssignmentComputePass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, ViewSet));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TileArraySSBO, LightClusterSet));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::LightUniformsUBO, LightClusterSet));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ClusterAABBsSSBO, ClusterGridSet));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ViewSpaceLightsSSBO, ViewSpaceLightsSet));
}

void TileAssignmentComputePass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.DeclareContexts<
        PassCtx::View,
        PassCtx::LightCluster,
        PassCtx::ClusterGrid>();

    auto viewSpaceLights = builder.DeclareStorageBuffer(RGResourceID::Custom, UBO::ViewSpaceLightsSSBOSize);
    builder.SetCustomResourceName(viewSpaceLights, "ViewSpaceLightsSSBO");
    builder.ReadStorageBuffer(viewSpaceLights, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ);

    auto tileBuffer = builder.DeclareStorageBuffer(RGResourceID::TileAssignmentBuffer, UBO::LightClusterSSBOSize);
    builder.WriteStorageBuffer(tileBuffer, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_WRITE);
    builder.SetHasSideEffects();
}

void TileAssignmentComputePass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("TileAssignmentComputePass::RenderWithGraph");
    StartRenderPassProfilingScope(execCtx.pCmdBuffer);
    auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;

    if (!ctx.clusterGridDescriptor)
    {
        EndRenderPassProfilingScope(execCtx.pCmdBuffer);
        return;
    }

    m_pushConstants.clusterCount = renderState.clusterCount;
    m_pushConstants.nearFar = mathstl::Vector4(ctx.zNear, ctx.zFar, 0.0f, 0.0f);
    m_pushConstants.numLights = ctx.numLights;

    u32 numLights = ctx.numLights;
    u32 workgroupCount = (numLights + 127) / 128;
    workgroupCount = workgroupCount > 0 ? workgroupCount : 1;

    DescriptorSet::Ptr viewSpaceLightsDesc = data.pResourceManager->GetViewSpaceLightsDescriptorSet(ctx.currentFrame);

    GenericComputeDispatchCmd cmd(&m_pipeline, workgroupCount, 1, 1);
    cmd.descriptorSets = {ctx.sharedDataUBODescriptor, ctx.tileArraySSBODescriptor, ctx.clusterGridDescriptor, viewSpaceLightsDesc};
    cmd.SetPushConstants(0, m_pushConstants);
    execCtx.pCmdBuffer->RecordCommand(cmd);
    EndRenderPassProfilingScope(execCtx.pCmdBuffer);
}

void TileAssignmentComputePass::Render(const MainPassData& data,
                                       FrameRendererContext& ctx,
                                       CommandBuffer* pCmdBuffer)
{
    ScopedZone("TileAssignmentComputePass::Render");

    StartRenderPassProfilingScope(pCmdBuffer);

    auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;

    if (!ctx.clusterGridDescriptor)
    {
        EndRenderPassProfilingScope(pCmdBuffer);
        return;
    }

    m_pushConstants.clusterCount = renderState.clusterCount;
    m_pushConstants.nearFar = mathstl::Vector4(ctx.zNear, ctx.zFar, 0.0f, 0.0f);
    m_pushConstants.numLights = ctx.numLights;

    u32 numLights = ctx.numLights;
    u32 workgroupCount = (numLights + 127) / 128;
    workgroupCount = workgroupCount > 0 ? workgroupCount : 1;

    DescriptorSet::Ptr viewSpaceLightsDesc = data.pResourceManager->GetViewSpaceLightsDescriptorSet(ctx.currentFrame);

    GenericComputeDispatchCmd cmd(&m_pipeline, workgroupCount, 1, 1);
    cmd.descriptorSets = {ctx.sharedDataUBODescriptor, ctx.tileArraySSBODescriptor, ctx.clusterGridDescriptor, viewSpaceLightsDesc};
    cmd.SetPushConstants(0, m_pushConstants);
    pCmdBuffer->RecordCommand(cmd);

    EndRenderPassProfilingScope(pCmdBuffer);
}
