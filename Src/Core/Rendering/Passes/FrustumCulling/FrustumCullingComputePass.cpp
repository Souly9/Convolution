#include "FrustumCullingComputePass.h"
#include "../PassManager.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include "Core/Rendering/Core/Utils/DescriptorLayoutUtils.h"

// TODO: Define descriptor sets
#define ViewSet 0
#define ObjectSet 1 

using namespace RenderPasses;

FrustumCullingComputePass::FrustumCullingComputePass() : ConvolutionRenderPass("FrustumCullingComputePass")
{
    CreateSharedDescriptorLayout();
}

FrustumCullingComputePass::~FrustumCullingComputePass() = default;

void FrustumCullingComputePass::Init(RendererAttachmentInfo& attachmentInfo, 
                                     const SharedResourceManager& resourceManager)
{
    // Initialize pipelines
    BuildPipelines();
}

void FrustumCullingComputePass::BuildPipelines()
{
    // TODO: Change shader path
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
    // Build any necessary buffers here
}

void FrustumCullingComputePass::CreateSharedDescriptorLayout()
{
    // TODO: Define shared descriptors
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, ViewSet));
    // m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::StorageBuffer, ObjectSet));
}

void FrustumCullingComputePass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                                    FrameRendererContext& previousFrameCtx,
                                                    u32 thisFrameNum)
{
    // Rebuild data if needed
}

void FrustumCullingComputePass::Render(const MainPassData& data, 
                                       FrameRendererContext& ctx, 
                                       CommandBuffer* pCmdBuffer)
{
    StartRenderPassProfilingScope(pCmdBuffer);

    // Update push constants
    m_pushConstants.objectCount = 0; // TODO: Set meaningful values

    // Dispatch compute shader
    // TODO: Calculate group counts
    u32 groupCountX = 1;
    u32 groupCountY = 1;
    u32 groupCountZ = 1;

    {
        GenericComputeDispatchCmd cmd(&m_cullingPipeline, groupCountX, groupCountY, groupCountZ);
        // cmd.descriptorSets = {ctx.mainViewUBODescriptor, ...};
        cmd.SetPushConstants(0, m_pushConstants);
        pCmdBuffer->RecordCommand(cmd);
    }

    EndRenderPassProfilingScope(pCmdBuffer);
}
