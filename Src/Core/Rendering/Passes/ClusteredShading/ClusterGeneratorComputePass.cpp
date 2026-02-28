#include "ClusterGeneratorComputePass.h"
#include "../PassManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"

#define ViewSet             0
#define LightClusterSet     1
#define ClusterGridSet      2
#define ViewSpaceLightsSet  3

using namespace RenderPasses;

ClusterGeneratorComputePass::ClusterGeneratorComputePass() : ConvolutionRenderPass("ClusterGeneratorComputePass")
{
    CreateSharedDescriptorLayout();
}

void ClusterGeneratorComputePass::Init(RendererAttachmentInfo& attachmentInfo,
                                       const SharedResourceManager& resourceManager)
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
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, ViewSet));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TileArraySSBO, LightClusterSet));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::LightUniformsUBO, LightClusterSet));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ClusterAABBsSSBO, ClusterGridSet));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ViewSpaceLightsSSBO, ViewSpaceLightsSet));
}

void ClusterGeneratorComputePass::Render(const MainPassData& data,
                                         FrameRendererContext& ctx,
                                         CommandBuffer* pCmdBuffer)
{
    ScopedZone("ClusterGeneratorComputePass::Render");

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

    const u32 workgroupsX = (m_pushConstants.clusterCount.x + 7) / 8;
    const u32 workgroupsY = (m_pushConstants.clusterCount.y + 7) / 8;
    const u32 workgroupsZ = m_pushConstants.clusterCount.z;

    GenericComputeDispatchCmd cmd(&m_pipeline, workgroupsX, workgroupsY, workgroupsZ);
    
    DescriptorSet* viewSpaceLightsDesc = data.pResourceManager->GetViewSpaceLightsDescriptorSet(ctx.currentFrame);
    
    cmd.descriptorSets = {ctx.sharedDataUBODescriptor, ctx.tileArraySSBODescriptor, ctx.clusterGridDescriptor, viewSpaceLightsDesc};
    cmd.SetPushConstants(0, m_pushConstants);
    pCmdBuffer->RecordCommand(cmd);

    EndRenderPassProfilingScope(pCmdBuffer);
}
