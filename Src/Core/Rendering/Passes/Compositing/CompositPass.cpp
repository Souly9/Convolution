#include "CompositPass.h"

RenderPasses::CompositPass::CompositPass() : GenericGeometryPass("CompositPass")
{
    SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates::Complete);
    CreateSharedDescriptorLayout();
}

void RenderPasses::CompositPass::Init(RendererAttachmentInfo& attachmentInfo,
                                      const SharedResourceManager& resourceManager)
{
    ScopedZone("CompositPass::Init");
    const auto& gbufferInfo = attachmentInfo.gbuffer;

    const auto swapChainAttachment = CreateDefaultColorAttachment(SWAPCHAINFORMAT, LoadOp::CLEAR, nullptr);
    m_mainRenderingData.colorAttachments = {swapChainAttachment};

    InitBaseData(attachmentInfo);
    m_indirectCmdBuffer = IndirectDrawCommandBuffer(10);
    AsyncQueueHandler::MeshTransfer cmd{};
    cmd.name = "CompositPass_MeshTransfer";
    cmd.pBuffersToFill = &m_mainRenderingData;
    UBO::PerPassObjectDataSSBO data{};
    GenericGeometryPass::DrawCmdOffsets offsets{};

    BuildPipelines();
}

void RenderPasses::CompositPass::BuildPipelines()
{
    ScopedZone("CompositPass::BuildPipelines");
    auto mainVert = Shader("Shaders/Simple.vert.spv", "main");
    auto mainFrag = Shader("Shaders/Simple.frag.spv", "main");

    PipelineInfo info{};
    info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
    info.attachmentInfos = CreateAttachmentInfo({m_mainRenderingData.colorAttachments});
    info.hasDepth = false;
    m_mainPSO = PSO(
        ShaderCollection{&mainVert, &mainFrag}, PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions}, info);
}

void RenderPasses::CompositPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                                     FrameRendererContext& previousFrameCtx,
                                                     u32 thisFrameNum)
{
    m_indirectCmdBuffer.EmptyCmds();
    const auto pFullScreenQuadMesh = g_pMeshManager->GetPrimitiveMesh(MeshManager::PrimitiveType::Quad);
    const auto meshHandle = previousFrameCtx.pResourceManager->GetMeshHandle(pFullScreenQuadMesh);
    m_indirectCmdBuffer.AddIndexedDrawCmd(
        meshHandle.indexCount, 1, meshHandle.indexBufferOffset, meshHandle.vertBufferOffset, 0);
    RebuildPerObjectBuffer({0});
    m_indirectCmdBuffer.FillCmds();
}

void RenderPasses::CompositPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    const auto currentFrame = ctx.currentFrame;
    UpdateContextForFrame(currentFrame);
    const auto& passCtx = m_perObjectFrameContexts[currentFrame];

    ColorAttachment swapchainAttachment = m_mainRenderingData.colorAttachments[0];
    swapchainAttachment.SetTexture(ctx.pCurrentSwapchainTexture);

    stltype::vector<ColorAttachment> colorAttachments = {swapchainAttachment};

    const auto ex = ctx.pCurrentSwapchainTexture->GetInfo().extents;
    const DirectX::XMINT2 extents(ex.x, ex.y);

    BeginRenderingCmd cmdBegin{&m_mainPSO, colorAttachments, nullptr};
    cmdBegin.extents = extents;
    cmdBegin.viewport = data.mainView.viewport;

    auto& sceneGeometryBuffers = data.pResourceManager->GetSceneGeometryBuffers();
    if (sceneGeometryBuffers.GetVertexBuffer().GetRef() == VK_NULL_HANDLE ||
        sceneGeometryBuffers.GetIndexBuffer().GetRef() == VK_NULL_HANDLE)
    {
        return;
    }
    BinRenderDataCmd geomBufferCmd(sceneGeometryBuffers.GetVertexBuffer(), sceneGeometryBuffers.GetIndexBuffer());

    GenericIndirectDrawCmd cmd{&m_mainPSO, m_indirectCmdBuffer};
    cmd.drawCount = m_indirectCmdBuffer.GetDrawCmdNum();

    if (data.bufferDescriptors.empty() == false)
    {
        const auto transformSSBOSet = data.bufferDescriptors.at(UBO::DescriptorContentsType::GlobalInstanceData);
        const auto texArraySet = data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessTextureArray);
        const auto tileArraySSBOSet = data.bufferDescriptors.at(UBO::DescriptorContentsType::LightData);
        const auto gbufferUBO = data.bufferDescriptors.at(UBO::DescriptorContentsType::GBuffer);
        cmd.descriptorSets = {texArraySet,
                              data.mainView.descriptorSet,
                              transformSSBOSet,
                              tileArraySSBOSet,
                              gbufferUBO,
                              ctx.shadowViewUBODescriptor};
    }
    StartRenderPassProfilingScope(pCmdBuffer);
    pCmdBuffer->RecordCommand(cmdBegin);
    pCmdBuffer->RecordCommand(geomBufferCmd);
    pCmdBuffer->RecordCommand(cmd);
    pCmdBuffer->RecordCommand(EndRenderingCmd{});
    EndRenderPassProfilingScope(pCmdBuffer);
}

void RenderPasses::CompositPass::CreateSharedDescriptorLayout()
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

bool RenderPasses::CompositPass::WantsToRender() const
{
    return true;
}
