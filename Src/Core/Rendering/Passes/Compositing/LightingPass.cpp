#include "LightingPass.h"
#include "Core/Rendering/Passes/GBuffer.h"

using namespace RenderPasses;

LightingPass::LightingPass() : GenericGeometryPass("LightingPass")
{
    SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates::Complete);
    CreateSharedDescriptorLayout();
}

void LightingPass::Init(RendererAttachmentInfo& attachmentInfo,
                                      const SharedResourceManager& resourceManager)
{
    ScopedZone("LightingPass::Init");
    const auto& gbufferInfo = attachmentInfo.gbuffer;
    const auto gbufferColorAttachment = CreateDefaultColorAttachment(gbufferInfo.GetFormat(GBufferTextureType::GBufferThisFrameColor), LoadOp::LOAD, nullptr);
    m_mainRenderingData.colorAttachments = {gbufferColorAttachment};

    InitBaseData(attachmentInfo);
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
        m_indirectCmdBuffers[i].Init(10);
    AsyncQueueHandler::MeshTransfer cmd{};
    cmd.pBuffersToFill = &m_mainRenderingData;

    BuildPipelines();
}

void LightingPass::BuildPipelines()
{
    ScopedZone("LightingPass::BuildPipelines");
    auto mainVert = Shader("Shaders/LightingPass.vert.spv", "main");
    auto mainFrag = Shader("Shaders/LightingPass.frag.spv", "main");

    PipelineInfo info{};
    info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
    info.attachmentInfos = CreateAttachmentInfo({m_mainRenderingData.colorAttachments});
    info.hasDepth = false;
    m_mainPSO = PSO(
        ShaderCollection{&mainVert, &mainFrag}, PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions}, info);
}

void LightingPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                                     FrameRendererContext& previousFrameCtx,
                                                     u32 thisFrameNum)
{
    m_currentFrameIdx = thisFrameNum % SWAPCHAIN_IMAGES;
    auto& cmdBuf = m_indirectCmdBuffers.at(m_currentFrameIdx);
    cmdBuf.EmptyCmds();
    const auto pFullScreenQuadMesh = g_pMeshManager->GetPrimitiveMesh(MeshManager::PrimitiveType::Quad);
    const auto meshHandle = previousFrameCtx.pResourceManager->GetMeshHandle(pFullScreenQuadMesh);
    cmdBuf.AddIndexedDrawCmd(
        meshHandle.indexCount, 1, meshHandle.indexBufferOffset, meshHandle.vertBufferOffset, 0);
    RebuildPerObjectBuffer({0});
    cmdBuf.FillCmds();
}

void LightingPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    const auto currentFrame = ctx.imageIdx;
    UpdateContextForFrame(currentFrame);
    const auto& passCtx = m_perObjectFrameContexts[currentFrame];

    ColorAttachment gbufferAttachment = m_mainRenderingData.colorAttachments[0];
    gbufferAttachment.SetTexture(data.pGbuffer->Get(GBufferTextureType::GBufferThisFrameColor));

    stltype::vector<ColorAttachment> colorAttachments = {gbufferAttachment};

    const DirectX::XMINT2 extents(data.renderState.renderResolution.x, data.renderState.renderResolution.y);

    BeginRenderingCmd cmdBegin{&m_mainPSO, ToRenderAttachmentInfos(colorAttachments)};
    cmdBegin.extents = extents;
    cmdBegin.viewport = data.mainView.viewport;

    auto& sceneGeometryBuffers = data.pResourceManager->GetSceneGeometryBuffers();
    if (sceneGeometryBuffers.GetVertexBuffer().GetRef() == VK_NULL_HANDLE ||
        sceneGeometryBuffers.GetIndexBuffer().GetRef() == VK_NULL_HANDLE)
    {
        return;
    }
    BinRenderDataCmd geomBufferCmd(sceneGeometryBuffers.GetVertexBuffer(), sceneGeometryBuffers.GetIndexBuffer());
    
    auto& cmdBuf = m_indirectCmdBuffers[m_currentFrameIdx];
    GenericIndirectDrawCmd cmd{&m_mainPSO, cmdBuf};
    cmd.drawCount = cmdBuf.GetDrawCmdNum();

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

void LightingPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalArrayTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, 1));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TransformSSBO, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GlobalObjectDataSSBOs, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::InstanceDataSSBO, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PrevTransformSSBO, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TileArraySSBO, 3));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::LightUniformsUBO, 3));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GBufferUBO, 4));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ShadowmapUBO, 4));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ShadowmapViewUBO, 5));
    // m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PerPassObjectSSBO, 4));
}

bool LightingPass::WantsToRender() const
{
    return true;
}
