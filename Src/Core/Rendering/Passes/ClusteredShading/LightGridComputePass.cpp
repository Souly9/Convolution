#include "LightGridComputePass.h"
#include "../PassManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include "Core/Rendering/Core/Utils/DescriptorLayoutUtils.h"


#define ViewSet         0
#define LightClusterSet 1
#define ClusterGridSet  2

using namespace RenderPasses;



LightGridComputePass::LightGridComputePass() : ConvolutionRenderPass("LightGridComputePass")
{
    CreateSharedDescriptorLayout();
}

LightGridComputePass::~LightGridComputePass() = default;

void LightGridComputePass::Init(RendererAttachmentInfo& attachmentInfo,
                                              const SharedResourceManager& resourceManager)
{
    ScopedZone("LightGridComputePass::Init");
    BuildPipelines();
}

void LightGridComputePass::BuildPipelines()
{
    ScopedZone("LightGridComputePass::BuildPipelines");

    auto cullingShader = Shader("Shaders/ClusteredLightCulling.comp.spv", "main");
    auto clusterShader = Shader("Shaders/ClusterGenerator.comp.spv", "main");

    ShaderCollection shaders{};
    shaders.pComputeShader = &cullingShader;

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst;
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(ClusterPushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    m_lightCullingComputePipeline = ComputePipeline(shaders, pipeInfo);

    shaders.pComputeShader = &clusterShader;
    m_clusterComputePipeline = ComputePipeline(shaders, pipeInfo);
}

void LightGridComputePass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, ViewSet));

    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TileArraySSBO, LightClusterSet));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::LightUniformsUBO, LightClusterSet));

    // Cluster grid SSBO (managed by PassManager)
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ClusterAABBsSSBO, ClusterGridSet));
}

void LightGridComputePass::Render(const MainPassData& data,
                                                FrameRendererContext& ctx,
                                                CommandBuffer* pCmdBuffer)
{
    ScopedZone("LightGridComputePass::Render");

    StartRenderPassProfilingScope(pCmdBuffer);

    auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;

    // Update stats for debug display (always, even if we can't dispatch)
    const u32 totalClusters = renderState.clusterCount.x * renderState.clusterCount.y * renderState.clusterCount.z;
    g_pApplicationState->RegisterUpdateFunction([totalClusters](ApplicationState& state)
                                                { state.renderState.totalClusterCount = totalClusters; });

    // Skip if no cluster grid descriptor available
    if (!ctx.clusterGridDescriptor)
    {
        EndRenderPassProfilingScope(pCmdBuffer);
        return;
    }

    m_pushConstants.clusterCount = renderState.clusterCount;
    m_pushConstants.nearFar = mathstl::Vector4(ctx.zNear, ctx.zFar, 0.0f, 0.0f);

    // Dispatch: one workgroup per cluster slice (Z), threads handle X*Y (8x8 workgroup)
    const u32 workgroupsX = (m_pushConstants.clusterCount.x + 7) / 8;
    const u32 workgroupsY = (m_pushConstants.clusterCount.y + 7) / 8;
    const u32 workgroupsZ = m_pushConstants.clusterCount.z;

    // Create AABBs for each cluster
    {
        GenericComputeDispatchCmd cmd(&m_clusterComputePipeline, workgroupsX, workgroupsY, workgroupsZ);
        cmd.descriptorSets = {ctx.mainViewUBODescriptor, ctx.tileArraySSBODescriptor, ctx.clusterGridDescriptor};
        cmd.SetPushConstants(0, m_pushConstants);
        pCmdBuffer->RecordCommand(cmd);
    }

    // Barrier: Wait for AABB generation to finish before culling
    {
        GlobalBarrierCmd barrier(SyncStages::COMPUTE_SHADER, SyncStages::COMPUTE_SHADER, 
                                 VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        pCmdBuffer->RecordCommand(barrier);
    }
    
    // Cull lights for each cluster
    {
        GenericComputeDispatchCmd cmd(&m_lightCullingComputePipeline, workgroupsX, workgroupsY, workgroupsZ);
        cmd.descriptorSets = {ctx.mainViewUBODescriptor, ctx.tileArraySSBODescriptor, ctx.clusterGridDescriptor};
        cmd.SetPushConstants(0, m_pushConstants);
        pCmdBuffer->RecordCommand(cmd);
    }

    EndRenderPassProfilingScope(pCmdBuffer);
}
