#include "LightGridComputePass.h"

RenderPasses::LightGridComputePass::LightGridComputePass() : GenericGeometryPass("LightGridComputePass")
{
    CreateSharedDescriptorLayout();
}

void RenderPasses::LightGridComputePass::Init(RendererAttachmentInfo& attachmentInfo,
                                              const SharedResourceManager& resourceManager)
{
    ScopedZone("LightGridComputePass::Init");
    const auto& gbufferInfo = attachmentInfo.gbuffer;

    const auto swapChainAttachment = CreateDefaultColorAttachment(SWAPCHAINFORMAT, LoadOp::CLEAR, nullptr);
    m_mainRenderingData.colorAttachments = {swapChainAttachment};

    BuildPipelines();
}

void RenderPasses::LightGridComputePass::BuildPipelines()
{
    ScopedZone("LightGridComputePass::BuildPipelines");
}

void RenderPasses::LightGridComputePass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                                             FrameRendererContext& previousFrameCtx,
                                                             u32 thisFrameNum)
{
}

void RenderPasses::LightGridComputePass::Render(const MainPassData& data,
                                                FrameRendererContext& ctx,
                                                CommandBuffer* pCmdBuffer)
{
}

void RenderPasses::LightGridComputePass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalArrayTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, 1));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TransformSSBO, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GlobalObjectDataSSBOs, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::InstanceDataSSBO, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TileArraySSBO, 3));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::LightUniformsUBO, 3));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GBufferUBO, 4));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ShadowmapUBO, 4));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ShadowmapViewUBO, 5));
    // m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PerPassObjectSSBO, 4));
}

bool RenderPasses::LightGridComputePass::WantsToRender() const
{
    return NeedToRender(m_indirectCmdBuffer);
}
