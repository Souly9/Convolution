#include "LightTransformComputePass.h"
#include "../PassManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/DescriptorUtils/DescriptorLayoutUtils.h"
#include "Core/Rendering/Core/RenderGraph/PassContext.h"
#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"

#define ViewSet             0
#define LightClusterSet     1
#define ClusterGridSet      2
#define ViewSpaceLightsSet  3

using namespace RenderPasses;

LightTransformComputePass::LightTransformComputePass() : ConvolutionRenderPass("LightTransformComputePass")
{
    CreateSharedDescriptorLayout();
}

void LightTransformComputePass::Init(const SharedResourceManager& resourceManager)
{
    ScopedZone("LightTransformComputePass::Init");
    BuildBuffers();
    BuildPipelines();
}

void LightTransformComputePass::BuildBuffers()
{
}

void LightTransformComputePass::BuildPipelines()
{
    ScopedZone("LightTransformComputePass::BuildPipelines");

    auto transformShader = Shader("Shaders/LightTransform.comp.spv", "main");

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst;
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(ClusterPushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    ShaderCollection shaders{};
    shaders.pComputeShader = &transformShader;
    m_pipeline = ComputePipeline(shaders, pipeInfo);
}

void LightTransformComputePass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset({PipelineDescriptorLayout(UBO::BufferType::View, 0)});
    AppendLayoutPreset(DescriptorPresets::LightCluster(1));
    AppendLayoutPreset(DescriptorPresets::ClusterGrid(2));
    AppendLayoutPreset({PipelineDescriptorLayout(UBO::BufferType::ViewSpaceLightsSSBO, 3)});
}

void LightTransformComputePass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.DeclareContexts<
        PassCtx::ClusteredView,
        PassCtx::ClusteredLightCluster,
        PassCtx::ClusterGrid,
        PassCtx::ViewSpaceLights>();

    auto viewSpaceLights = builder.DeclareStorageBuffer(RGResourceID::ViewSpaceLightsBuffer, UBO::ViewSpaceLightsSSBOSize);
    builder.WriteStorageBuffer(viewSpaceLights, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_WRITE);
    builder.SetHasSideEffects();
}

void LightTransformComputePass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("LightTransformComputePass::RenderWithGraph");
    StartRenderPassProfilingScope(execCtx.pCmdBuffer);
    auto& renderState = g_engine.GetApplicationState().GetCurrentApplicationState().renderState;

    m_pushConstants.clusterCount = renderState.clusterCount;
    m_pushConstants.nearFar = mathstl::Vector4(execCtx.GetZNear(), execCtx.GetZFar(), 0.0f, 0.0f);
    m_pushConstants.numLights = execCtx.GetNumLights();

    u32 workgroupCount = (execCtx.GetNumLights() + 255) / 256;
    workgroupCount = workgroupCount > 0 ? workgroupCount : 1;
    GenericComputeDispatchCmd cmd(&m_pipeline, workgroupCount, 1, 1);

    cmd.descriptorSets = execCtx.GetDescriptors();
    cmd.SetPushConstants(0, m_pushConstants);
    execCtx.pCmdBuffer->RecordCommand(cmd);
    EndRenderPassProfilingScope(execCtx.pCmdBuffer);
}

