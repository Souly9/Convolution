#include "LightGridComputePass.h"
#include "../PassManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/Shader.h"
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

LightGridComputePass::LightGridComputePass() : ConvolutionRenderPass("LightGridComputePass")
{
    CreateSharedDescriptorLayout();
}

LightGridComputePass::~LightGridComputePass() = default;

void LightGridComputePass::Init(const SharedResourceManager& resourceManager)
{
    ScopedZone("LightGridComputePass::Init");
    BuildBuffers();
    BuildPipelines();
}

void LightGridComputePass::BuildBuffers()
{
}

void LightGridComputePass::BuildPipelines()
{
    ScopedZone("LightGridComputePass::BuildPipelines");

    auto cullingShader   = Shader("Shaders/ClusteredLightCulling.comp.spv", "main");

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst;
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(ClusterPushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    ShaderCollection shaders{};
    shaders.pComputeShader = &cullingShader;
    m_lightCullingComputePipeline = ComputePipeline(shaders, pipeInfo);
}

void LightGridComputePass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset({PipelineDescriptorLayout(UBO::BufferType::View, 0)});
    AppendLayoutPreset(DescriptorPresets::LightCluster(1));
    AppendLayoutPreset(DescriptorPresets::ClusterGrid(2));
    AppendLayoutPreset({PipelineDescriptorLayout(UBO::BufferType::ViewSpaceLightsSSBO, 3)});
}

void LightGridComputePass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.DeclareContexts<
        PassCtx::ClusteredView,
        PassCtx::ClusteredLightCluster,
        PassCtx::ClusterGrid,
        PassCtx::ViewSpaceLights>();

    // Reads the tiles, adds to the cluster light total and copies that total out for the stats UI
    auto viewSpaceLights = builder.DeclareStorageBuffer(RGResourceID::ViewSpaceLightsBuffer, UBO::ViewSpaceLightsSSBOSize);
    builder.WriteStorageBuffer(viewSpaceLights,
                               SyncStages::COMPUTE_SHADER | SyncStages::TRANSFER,
                               AccessFlags::SHADER_READ | AccessFlags::SHADER_WRITE | AccessFlags::TRANSFER_READ);
    auto clusterGrid = builder.DeclareStorageBuffer(RGResourceID::ClusterGridBuffer, UBO::ClusterAABBSetSize);
    builder.ReadStorageBuffer(clusterGrid, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ);
    // Writes the per-cluster offsets and light lists
    auto lightCluster = builder.DeclareStorageBuffer(RGResourceID::LightClusterBuffer, UBO::LightClusterSSBOSize);
    builder.WriteStorageBuffer(lightCluster, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_WRITE);
    builder.SetHasSideEffects();
}

void LightGridComputePass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("LightGridComputePass::RenderWithGraph");
    StartRenderPassProfilingScope(execCtx.pCmdBuffer);

    auto& renderState = g_engine.GetApplicationState().GetCurrentApplicationState().renderState;

    const u32 totalClusters = renderState.clusterCount.x * renderState.clusterCount.y * renderState.clusterCount.z;
    const u32 numLightsEvaluated = execCtx.GetNumLights();
    g_engine.GetApplicationState().RegisterUpdateFunction([totalClusters, numLightsEvaluated](ApplicationState& state)
                                                {
                                                    state.renderState.totalClusterCount = totalClusters;
                                                    state.renderState.numLightsEvaluated = numLightsEvaluated;
                                                });

    m_pushConstants.clusterCount = renderState.clusterCount;
    m_pushConstants.nearFar = mathstl::Vector4(execCtx.GetZNear(), execCtx.GetZFar(), 0.0f, 0.0f);
    m_pushConstants.numLights = execCtx.GetNumLights();

    const u32 workgroupsX = static_cast<u32>(m_pushConstants.clusterCount.x);
    const u32 workgroupsY = static_cast<u32>(m_pushConstants.clusterCount.y);
    const u32 workgroupsZ = 1;

    GenericComputeDispatchCmd cmd(&m_lightCullingComputePipeline, workgroupsX, workgroupsY, workgroupsZ);
    cmd.descriptorSets = execCtx.GetDescriptors();
    cmd.SetPushConstants(0, m_pushConstants);
    execCtx.pCmdBuffer->RecordCommand(cmd);

    // The CPU reads this slot's copy after the slot's fence, so the stats never stall the frame
    execCtx.pCmdBuffer->RecordCommand(GlobalBarrierCmd(
        SyncStages::COMPUTE_SHADER, SyncStages::TRANSFER, AccessFlags::SHADER_WRITE, AccessFlags::TRANSFER_READ));
    SimpleBufferCopyCmd statsCopy{&data.pResourceManager->GetViewSpaceLightsSSBO(),
                                  &data.pResourceManager->GetClusterStatsReadback(execCtx.GetFrameIndex())};
    statsCopy.srcOffset = UBO::ViewSpaceLights_ClusterLightTotalOffset;
    statsCopy.size = sizeof(u32);
    execCtx.pCmdBuffer->RecordCommand(statsCopy);
    // A fence wait alone doesn't make device writes visible to the host
    execCtx.pCmdBuffer->RecordCommand(
        GlobalBarrierCmd(SyncStages::TRANSFER, SyncStages::HOST, AccessFlags::TRANSFER_WRITE, AccessFlags::HOST_READ));
    EndRenderPassProfilingScope(execCtx.pCmdBuffer);
}

