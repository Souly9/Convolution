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

void TileAssignmentComputePass::Init(const SharedResourceManager& resourceManager)
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
    m_sharedDescriptors.clear();
    AppendLayoutPreset({PipelineDescriptorLayout(UBO::BufferType::View, 0)});
    AppendLayoutPreset(DescriptorPresets::LightCluster(1));
    AppendLayoutPreset(DescriptorPresets::ClusterGrid(2));
    AppendLayoutPreset({PipelineDescriptorLayout(UBO::BufferType::ViewSpaceLightsSSBO, 3)});
}

void TileAssignmentComputePass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.DeclareContexts<
        PassCtx::ClusteredView,
        PassCtx::ClusteredLightCluster,
        PassCtx::ClusterGrid,
        PassCtx::ViewSpaceLights>();

    // Reads the view-space lights, clears the tile counters with a transfer and appends to the tiles with atomics
    auto viewSpaceLights = builder.DeclareStorageBuffer(RGResourceID::ViewSpaceLightsBuffer, UBO::ViewSpaceLightsSSBOSize);
    builder.WriteStorageBuffer(viewSpaceLights,
                               SyncStages::COMPUTE_SHADER | SyncStages::TRANSFER,
                               AccessFlags::SHADER_READ | AccessFlags::SHADER_WRITE | AccessFlags::TRANSFER_WRITE);
    builder.SetHasSideEffects();
}

void TileAssignmentComputePass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("TileAssignmentComputePass::RenderWithGraph");
    StartRenderPassProfilingScope(execCtx.pCmdBuffer);
    auto& renderState = g_engine.GetApplicationState().GetCurrentApplicationState().renderState;

    m_pushConstants.clusterCount = renderState.clusterCount;
    m_pushConstants.nearFar = mathstl::Vector4(execCtx.GetZNear(), execCtx.GetZFar(), 0.0f, 0.0f);
    m_pushConstants.numLights = execCtx.GetNumLights();

    u32 numLights = execCtx.GetNumLights();
    u32 workgroupCount = (numLights + 127) / 128;
    workgroupCount = workgroupCount > 0 ? workgroupCount : 1;

    // The shader only appends, so the counters restart every frame
    execCtx.pCmdBuffer->RecordCommand(BufferFillCmd(&data.pResourceManager->GetViewSpaceLightsSSBO(),
                                                    UBO::ViewSpaceLights_LightsSize,
                                                    UBO::ViewSpaceLights_TileCountersSize));
    execCtx.pCmdBuffer->RecordCommand(GlobalBarrierCmd(SyncStages::TRANSFER,
                                                       SyncStages::COMPUTE_SHADER,
                                                       AccessFlags::TRANSFER_WRITE,
                                                       AccessFlags::SHADER_READ | AccessFlags::SHADER_WRITE));

    GenericComputeDispatchCmd cmd(&m_pipeline, workgroupCount, 1, 1);
    cmd.descriptorSets = execCtx.GetDescriptors();
    cmd.SetPushConstants(0, m_pushConstants);
    execCtx.pCmdBuffer->RecordCommand(cmd);
    EndRenderPassProfilingScope(execCtx.pCmdBuffer);
}

