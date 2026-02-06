#include "LightGridComputePass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include "Core/Rendering/Core/Utils/DescriptorLayoutUtils.h"
#include "../PassManager.h"

#define ViewSet         0
#define LightClusterSet 1
#define ClusterGridSet  2

using namespace RenderPasses;

RenderPasses::LightGridComputePass::LightGridComputePass() : ConvolutionRenderPass("LightGridComputePass")
{
    CreateSharedDescriptorLayout();
}

RenderPasses::LightGridComputePass::~LightGridComputePass() = default;

void RenderPasses::LightGridComputePass::Init(RendererAttachmentInfo& attachmentInfo,
                                              const SharedResourceManager& resourceManager)
{
    ScopedZone("LightGridComputePass::Init");
    BuildPipelines();
}

void RenderPasses::LightGridComputePass::BuildPipelines()
{
    ScopedZone("LightGridComputePass::BuildPipelines");

    auto computeShader = Shader("Shaders/ClusteredLightCulling.comp.spv", "main");

    ShaderCollection shaders{};
    shaders.pVertShader = nullptr;
    shaders.pFragShader = nullptr;
    shaders.pComputeShader = &computeShader;

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst;
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(ClusterPushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    m_pComputePipeline = new ComputePipeline(shaders, pipeInfo);
}

void RenderPasses::LightGridComputePass::CreateSharedDescriptorLayout()
{
    // Set 0: View UBO
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, ViewSet));

    // Set 1: Light cluster data (TileArraySSBO)
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TileArraySSBO, LightClusterSet));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::LightUniformsUBO, LightClusterSet));

    // Set 2: Cluster grid SSBO (managed by PassManager)
    PipelineDescriptorLayout clusterGridLayout{};
    clusterGridLayout.type = DescriptorType::StorageBuffer;
    clusterGridLayout.setIndex = ClusterGridSet;
    clusterGridLayout.bindingSlot = s_clusterGridSSBOBindingSlot;
    clusterGridLayout.shaderStagesToBind = ShaderTypeBits::Compute;
    m_sharedDescriptors.emplace_back(clusterGridLayout);
}

void RenderPasses::LightGridComputePass::Render(const MainPassData& data,
                                                FrameRendererContext& ctx,
                                                CommandBuffer* pCmdBuffer)
{
    ScopedZone("LightGridComputePass::Render");

    if (!m_pComputePipeline)
        return;

    StartRenderPassProfilingScope(pCmdBuffer);

    auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    m_pushConstants.clusterCount = renderState.clusterCount;
    m_pushConstants.nearFar = mathstl::Vector4(ctx.zNear, ctx.zFar, 0.0f, 0.0f);

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

    // Dispatch: one workgroup per cluster slice (Z), threads handle X*Y (8x8 workgroup)
    const u32 workgroupsX = (m_pushConstants.clusterCount.x + 7) / 8;
    const u32 workgroupsY = (m_pushConstants.clusterCount.y + 7) / 8;
    const u32 workgroupsZ = m_pushConstants.clusterCount.z;

    GenericComputeDispatchCmd cmd(m_pComputePipeline, workgroupsX, workgroupsY, workgroupsZ);
    cmd.descriptorSets = {ctx.mainViewUBODescriptor, ctx.tileArraySSBODescriptor, ctx.clusterGridDescriptor};
    cmd.SetPushConstants(0, m_pushConstants);

    pCmdBuffer->RecordCommand(cmd);

    EndRenderPassProfilingScope(pCmdBuffer);
}
