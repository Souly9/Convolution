#include "LightTransformComputePass.h"
#include "../PassManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/Utils/DescriptorLayoutUtils.h"

#define ViewSet             0
#define LightClusterSet     1
#define ClusterGridSet      2
#define ViewSpaceLightsSet  3

using namespace RenderPasses;

LightTransformComputePass::LightTransformComputePass() : ConvolutionRenderPass("LightTransformComputePass")
{
    CreateSharedDescriptorLayout();
}

void LightTransformComputePass::Init(RendererAttachmentInfo& attachmentInfo,
                                     const SharedResourceManager& resourceManager)
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
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, ViewSet));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TileArraySSBO, LightClusterSet));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::LightUniformsUBO, LightClusterSet));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ClusterAABBsSSBO, ClusterGridSet));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ViewSpaceLightsSSBO, ViewSpaceLightsSet));
}

void LightTransformComputePass::Render(const MainPassData& data,
                                       FrameRendererContext& ctx,
                                       CommandBuffer* pCmdBuffer)
{
    ScopedZone("LightTransformComputePass::Render");

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

    u32 workgroupCount = (ctx.numLights + 255) / 256;
    workgroupCount = workgroupCount > 0 ? workgroupCount : 1;
    GenericComputeDispatchCmd cmd(&m_pipeline, workgroupCount, 1, 1);

    DescriptorSet* viewSpaceLightsDesc = data.pResourceManager->GetViewSpaceLightsDescriptorSet(ctx.currentFrame);
    
    cmd.descriptorSets = {ctx.sharedDataUBODescriptor, ctx.tileArraySSBODescriptor, ctx.clusterGridDescriptor, viewSpaceLightsDesc};
    cmd.SetPushConstants(0, m_pushConstants);
    pCmdBuffer->RecordCommand(cmd);

    EndRenderPassProfilingScope(pCmdBuffer);
}
