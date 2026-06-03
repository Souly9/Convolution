#include "StaticMeshPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Utils/RenderPassUtils.h"

using namespace RenderPasses;

void StaticMainMeshPass::BuildBuffers()
{
}

StaticMainMeshPass::StaticMainMeshPass() : GenericGeometryPass("StaticMainMeshPass")
{
    SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates::Complete);
    CreateSharedDescriptorLayout();
}

void StaticMainMeshPass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
{
    ScopedZone("StaticMeshPass::Init");

    RecreateResolutionDependentResources(attachmentInfo, resourceManager);
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
        m_indirectCmdBuffers[i].Init(1000000);
    BuildPipelines();
}

void StaticMainMeshPass::RecreateResolutionDependentResources(RendererAttachmentInfo& attachmentInfo,
                                                              const SharedResourceManager& resourceManager)
{
    ScopedZone("StaticMeshPass::RecreateResolutionDependentResources");

    const auto& gbufferInfo = attachmentInfo.gbuffer;

    const auto gbufferPosition =
        CreateDefaultColorAttachment(gbufferInfo.GetFormat(GBufferTextureType::GBufferAlbedo), LoadOp::CLEAR, nullptr);
    const auto gbufferNormal =
        CreateDefaultColorAttachment(gbufferInfo.GetFormat(GBufferTextureType::GBufferNormal), LoadOp::CLEAR, nullptr);
    const auto gbuffer3 = CreateDefaultColorAttachment(
        gbufferInfo.GetFormat(GBufferTextureType::TexCoordMatData), LoadOp::CLEAR, nullptr);
    const auto gbufferVelocity = CreateDefaultColorAttachment(
        gbufferInfo.GetFormat(GBufferTextureType::GBufferVelocity), LoadOp::CLEAR, nullptr);
    const auto gbufferRoughness = CreateDefaultColorAttachment(
        gbufferInfo.GetFormat(GBufferTextureType::GBufferRoughness), LoadOp::CLEAR, nullptr);

    m_mainRenderingData.depthAttachment =
        CreateReadOnlyDepthAttachment(LoadOp::LOAD, attachmentInfo.depthAttachment.GetTexture());
    m_mainRenderingData.colorAttachments = {
        gbufferPosition, gbufferNormal, gbuffer3, gbufferVelocity, gbufferRoughness};

    InitBaseData(attachmentInfo);
}

void StaticMainMeshPass::BuildPipelines()
{
    ScopedZone("StaticMeshPass::BuildPipelines");

    auto mainVert = Shader("Shaders/GBufferPass.vert.spv", "main");
    auto mainFrag = Shader("Shaders/GBufferPass.frag.spv", "main");

    PipelineInfo info{};
    info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
    info.depthWriteEnable = false;
    info.attachmentInfos =
        CreateAttachmentInfo({m_mainRenderingData.colorAttachments}, m_mainRenderingData.depthAttachment);
    m_mainPSO = PSO(
        ShaderCollection{&mainVert, &mainFrag}, PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions}, info);
}

void StaticMainMeshPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                             FrameRendererContext& previousFrameCtx,
                                             u32 thisFrameNum)
{
    ScopedZone("StaticMeshPass::Rebuild");

    m_currentFrameIdx = thisFrameNum;
    auto& cmdBuf = m_indirectCmdBuffers[thisFrameNum];
    cmdBuf.EmptyCmds();
    u32 instanceOffset = 0;
    stltype::vector<u32> instanceDataIndices;
    instanceDataIndices.reserve(meshes.size());
    for (const auto& mesh : meshes)
    {
        if (mesh.meshData.IsDebugMesh())
            continue;
        const auto& meshHandle = mesh.meshData.meshResourceHandle;

        cmdBuf.AddIndexedDrawCmd(meshHandle.indexCount,
                                 1, // TODO: instanced rendering
                                 meshHandle.indexBufferOffset,
                                 meshHandle.vertBufferOffset,
                                 instanceOffset);
        instanceDataIndices.emplace_back(mesh.meshData.instanceDataIdx);
        ++instanceOffset;
    }
    RebuildPerObjectBuffer(instanceDataIndices);
    cmdBuf.FillCmds();
}

void StaticMainMeshPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    ScopedZone("StaticMeshPass::Render");

    const auto currentFrame = ctx.currentFrame;
    UpdateContextForFrame(currentFrame);
    const auto& passCtx = m_perObjectFrameContexts[currentFrame];

    ColorAttachment gbufferPosition = m_mainRenderingData.colorAttachments[0];
    ColorAttachment gbufferNormal = m_mainRenderingData.colorAttachments[1];
    ColorAttachment gbuffer3 = m_mainRenderingData.colorAttachments[2];
    ColorAttachment gbufferVelocity = m_mainRenderingData.colorAttachments[3];
    ColorAttachment gbufferRoughness = m_mainRenderingData.colorAttachments[4];
    gbufferPosition.SetTexture(data.pGbuffer->Get(GBufferTextureType::GBufferAlbedo));
    gbufferNormal.SetTexture(data.pGbuffer->Get(GBufferTextureType::GBufferNormal));
    gbuffer3.SetTexture(data.pGbuffer->Get(GBufferTextureType::TexCoordMatData));
    gbufferVelocity.SetTexture(data.pGbuffer->Get(GBufferTextureType::GBufferVelocity));
    gbufferRoughness.SetTexture(data.pGbuffer->Get(GBufferTextureType::GBufferRoughness));

    stltype::vector<ColorAttachment> colorAttachments = {
        gbufferPosition, gbufferNormal, gbuffer3, gbufferVelocity, gbufferRoughness};

    const DirectX::XMINT2 extents(data.renderState.renderResolution.x, data.renderState.renderResolution.y);

    m_mainRenderingData.depthAttachment.SetTexture(data.pMainDepthTexture);
    BeginRenderingCmd cmdBegin{&m_mainPSO,
                               ToRenderAttachmentInfos(colorAttachments),
                               ToRenderAttachmentInfo(m_mainRenderingData.depthAttachment)};
    cmdBegin.extents = extents;
    cmdBegin.viewport = data.mainView.viewport;

    auto& cmdBuf = m_indirectCmdBuffers[currentFrame];
    GenericIndirectDrawCmd cmd{&m_mainPSO, cmdBuf};
    cmd.drawCount = cmdBuf.GetDrawCmdNum();

    auto& sceneGeometryBuffers = data.pResourceManager->GetSceneGeometryBuffers();
    if (sceneGeometryBuffers.GetVertexBuffer().GetRef() == VK_NULL_HANDLE ||
        sceneGeometryBuffers.GetIndexBuffer().GetRef() == VK_NULL_HANDLE)
    {
        return;
    }

    if (data.bufferDescriptors.empty())
        cmd.descriptorSets = {DescriptorSet::Cast(g_pTexManager->GetBindlessDescriptorSet())};
    else
    {
        const auto transformSSBOSet = data.bufferDescriptors.at(UBO::DescriptorContentsType::GlobalInstanceData);
        const auto texArraySet = data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessTextureArray);
        cmd.descriptorSets = {
            texArraySet, data.mainView.descriptorSet, transformSSBOSet, passCtx.m_perObjectDescriptor};
    }
    cmdBegin.drawCmdBuffer = &cmdBuf;
    StartRenderPassProfilingScope(pCmdBuffer);
    pCmdBuffer->RecordCommand(cmdBegin);
    BinRenderDataCmd geomBufferCmd(sceneGeometryBuffers.GetVertexBuffer(), sceneGeometryBuffers.GetIndexBuffer());
    pCmdBuffer->RecordCommand(geomBufferCmd);
    pCmdBuffer->RecordCommand(cmd);
    pCmdBuffer->RecordCommand(EndRenderingCmd{});
    EndRenderPassProfilingScope(pCmdBuffer);
}

void StaticMainMeshPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset(DescriptorPresets::Bindless());
    AppendLayoutPreset(DescriptorPresets::View());
    AppendLayoutPreset(DescriptorPresets::GlobalInstanceData());
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PerPassObjectSSBO, 3));
}

bool StaticMainMeshPass::WantsToRender() const
{
    return NeedToRender(m_indirectCmdBuffers[m_currentFrameIdx]);
}
