#include "ClusterGeneratorComputePass.h"
#include "../PassManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/RenderGraph/PassContext.h"
#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"

#define ViewSet             0
#define LightClusterSet     1
#define ClusterGridSet      2
#define ViewSpaceLightsSet  3

using namespace RenderPasses;

ClusterGeneratorComputePass::ClusterGeneratorComputePass() : ConvolutionRenderPass("ClusterGeneratorComputePass")
{
    CreateSharedDescriptorLayout();
}

void ClusterGeneratorComputePass::Init(const SharedResourceManager& resourceManager)
{
    ScopedZone("ClusterGeneratorComputePass::Init");
    BuildBuffers();
    BuildPipelines();
}

void ClusterGeneratorComputePass::BuildBuffers()
{
}

void ClusterGeneratorComputePass::BuildPipelines()
{
    ScopedZone("ClusterGeneratorComputePass::BuildPipelines");

    auto clusterShader = Shader("Shaders/ClusterGenerator.comp.spv", "main");

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst;
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(ClusterPushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    ShaderCollection shaders{};
    shaders.pComputeShader = &clusterShader;
    m_pipeline = ComputePipeline(shaders, pipeInfo);
}

void ClusterGeneratorComputePass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset({PipelineDescriptorLayout(UBO::BufferType::View, 0)});
    AppendLayoutPreset(DescriptorPresets::LightCluster(1));
    AppendLayoutPreset(DescriptorPresets::ClusterGrid(2));
    AppendLayoutPreset({PipelineDescriptorLayout(UBO::BufferType::ViewSpaceLightsSSBO, 3)});
}

void ClusterGeneratorComputePass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.DeclareContexts<
        PassCtx::ClusteredView,
        PassCtx::ClusteredLightCluster,
        PassCtx::ClusterGrid,
        PassCtx::ViewSpaceLights>();

    auto viewSpaceLights = builder.DeclareStorageBuffer(RGResourceID::Custom, UBO::ViewSpaceLightsSSBOSize, "ViewSpaceLightsSSBO");
    builder.ReadStorageBuffer(viewSpaceLights, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ);

    auto tileBuffer = builder.DeclareStorageBuffer(RGResourceID::TileAssignmentBuffer, UBO::LightClusterSSBOSize);
    builder.ReadStorageBuffer(tileBuffer, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ);
    builder.SetHasSideEffects();
}

void ClusterGeneratorComputePass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("ClusterGeneratorComputePass::RenderWithGraph");
    StartRenderPassProfilingScope(execCtx.pCmdBuffer);
    auto& renderState = g_engine.GetApplicationState().GetCurrentApplicationState().renderState;

    m_pushConstants.clusterCount = renderState.clusterCount;
    m_pushConstants.nearFar = mathstl::Vector4(execCtx.GetZNear(), execCtx.GetZFar(), 0.0f, 0.0f);
    m_pushConstants.numLights = execCtx.GetNumLights();

    const u32 workgroupsX = (m_pushConstants.clusterCount.x + 7) / 8;
    const u32 workgroupsY = (m_pushConstants.clusterCount.y + 7) / 8;
    const u32 workgroupsZ = m_pushConstants.clusterCount.z;

    GenericComputeDispatchCmd cmd(&m_pipeline, workgroupsX, workgroupsY, workgroupsZ);
    cmd.descriptorSets = execCtx.GetDescriptors();
    cmd.SetPushConstants(0, m_pushConstants);
    execCtx.pCmdBuffer->RecordCommand(cmd);
    EndRenderPassProfilingScope(execCtx.pCmdBuffer);
}
