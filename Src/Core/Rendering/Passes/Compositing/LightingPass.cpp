#include "LightingPass.h"
#include "Core/Rendering/Core/Pipeline.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"

using namespace RenderPasses;

LightingPass::LightingPass() : ConvolutionRenderPass("LightingPass")
{
    CreateSharedDescriptorLayout();
}

void LightingPass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
{
    ScopedZone("LightingPass::Init");
    RecreateResolutionDependentResources(attachmentInfo, resourceManager);
    BuildPipelines();
}

void LightingPass::RecreateResolutionDependentResources(RendererAttachmentInfo& attachmentInfo,
                                                        const SharedResourceManager& resourceManager)
{
}

void LightingPass::BuildPipelines()
{
    ScopedZone("LightingPass::BuildPipelines");
    auto compShader = Shader("Shaders/LightingPass.comp.spv", "main");

    ShaderCollection shaders{};
    shaders.pComputeShader = &compShader;

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    m_computePipeline = ComputePipeline(shaders, pipeInfo);
}

void LightingPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    ScopedZone("LightingPass::Render");
    StartRenderPassProfilingScope(pCmdBuffer);

    const u32 groupCountX = (static_cast<u32>(data.renderState.renderResolution.x) + 7) / 8;
    const u32 groupCountY = (static_cast<u32>(data.renderState.renderResolution.y) + 7) / 8;
    const u32 groupCountZ = 1;

    GenericComputeDispatchCmd cmd(&m_computePipeline, groupCountX, groupCountY, groupCountZ);

    if (!data.bufferDescriptors.empty())
    {
        const auto transformSSBOSet = data.bufferDescriptors.at(UBO::DescriptorContentsType::GlobalInstanceData);
        const auto combinedBindlessSet = DescriptorSet::Cast(g_pTexManager->GetCombinedBindlessDescriptorSet());
        const auto tileArraySSBOSet = data.bufferDescriptors.at(UBO::DescriptorContentsType::LightData);
        const auto gbufferUBO = data.bufferDescriptors.at(UBO::DescriptorContentsType::GBuffer);

        cmd.descriptorSets = {combinedBindlessSet,
                              data.mainView.descriptorSet,
                              transformSSBOSet,
                              tileArraySSBOSet,
                              gbufferUBO};
    }

    pCmdBuffer->RecordCommand(cmd);
    EndRenderPassProfilingScope(pCmdBuffer);
}

void LightingPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalArrayTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalImages, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, 1));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TransformSSBO, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GlobalObjectDataSSBOs, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::InstanceDataSSBO, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PrevTransformSSBO, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TileArraySSBO, 3));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::LightUniformsUBO, 3));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GBufferUBO, 4));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ShadowmapUBO, 4));
}

bool LightingPass::WantsToRender() const
{
    return true;
}
