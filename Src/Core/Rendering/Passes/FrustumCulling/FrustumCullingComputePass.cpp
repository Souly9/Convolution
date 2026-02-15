#include "FrustumCullingComputePass.h"
#include "../PassManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include "Core/Rendering/Core/Utils/DescriptorLayoutUtils.h"
#include "Core/Global/Profiling.h"

using namespace RenderPasses;

FrustumCullingComputePass::FrustumCullingComputePass() : ConvolutionRenderPass("FrustumCullingComputePass")
{
    CreateSharedDescriptorLayout();
}

FrustumCullingComputePass::~FrustumCullingComputePass() = default;

void FrustumCullingComputePass::Init(RendererAttachmentInfo& attachmentInfo,
                                     const SharedResourceManager& resourceManager)
{
    ScopedZone("FrustumCullingComputePass::Init");
    BuildPipelines();
}

void FrustumCullingComputePass::BuildPipelines()
{
    auto computeShader = Shader("Shaders/FrustumCulling.comp.spv", "main");

    ShaderCollection shaders{};
    shaders.pComputeShader = &computeShader;

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst;
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(FrustumCullingPushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    m_cullingPipeline = ComputePipeline(shaders, pipeInfo);
}

void FrustumCullingComputePass::BuildBuffers()
{
}

void FrustumCullingComputePass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, 0));

    // Scene Object Data Set
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TransformSSBO, 1));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GlobalObjectDataSSBOs, 1));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::InstanceDataSSBO, 1));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::SceneAABBsSSBO, 2));
}

void FrustumCullingComputePass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                                    FrameRendererContext& previousFrameCtx,
                                                    u32 thisFrameNum)
{
}

void FrustumCullingComputePass::Render(const MainPassData& data,
                                       FrameRendererContext& ctx,
                                       CommandBuffer* pCmdBuffer)
{
    ScopedZone("FrustumCullingComputePass::Render");
    StartRenderPassProfilingScope(pCmdBuffer);

    // Get object count from shared resource manager
    u32 objectCount = (u32)ctx.pResourceManager->GetBufferOffsetData().instanceCount;
    if (objectCount == 0)
    {
        EndRenderPassProfilingScope(pCmdBuffer);
        return;
    }

    m_pushConstants.objectCount = objectCount;
    
    // Calculate dispatch groups (assuming local_size_x = 64)
    u32 groupCountX = (objectCount + 63) / 64;
    u32 groupCountY = 1;
    u32 groupCountZ = 1;

    {
        GenericComputeDispatchCmd cmd(&m_cullingPipeline, groupCountX, groupCountY, groupCountZ);
        cmd.descriptorSets = {ctx.mainViewUBODescriptor, ctx.pResourceManager->GetInstanceSSBODescriptorSet(ctx.imageIdx),
                              ctx.pResourceManager->GetSceneAABBSSBODescriptorSet(ctx.imageIdx)};
        cmd.SetPushConstants(0, m_pushConstants);
        //pCmdBuffer->RecordCommand(cmd);
    }

    EndRenderPassProfilingScope(pCmdBuffer);
}
