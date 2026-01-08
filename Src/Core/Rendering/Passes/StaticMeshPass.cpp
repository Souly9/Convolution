#include "StaticMeshPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "Utils/RenderPassUtils.h"

namespace RenderPasses
{
void StaticMainMeshPass::BuildBuffers()
{
}

StaticMainMeshPass::StaticMainMeshPass() : GenericGeometryPass("StaticMainMeshPass")
{
    SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates::Complete);
    CreateSharedDescriptorLayout();
}

void RenderPasses::StaticMainMeshPass::Init(RendererAttachmentInfo& attachmentInfo,
                                            const SharedResourceManager& resourceManager)
{
    ScopedZone("StaticMeshPass::Init");

    const auto& gbufferInfo = attachmentInfo.gbuffer;

    // const auto gbufferPosition = CreateDefaultColorAttachment(attachmentInfo.swapchainTextures[0].GetInfo().format,
    // LoadOp::CLEAR, nullptr);
    const auto gbufferPosition =
        CreateDefaultColorAttachment(gbufferInfo.GetFormat(GBufferTextureType::GBufferAlbedo), LoadOp::CLEAR, nullptr);
    const auto gbufferNormal =
        CreateDefaultColorAttachment(gbufferInfo.GetFormat(GBufferTextureType::GBufferNormal), LoadOp::CLEAR, nullptr);
    const auto gbuffer3 = CreateDefaultColorAttachment(
        gbufferInfo.GetFormat(GBufferTextureType::TexCoordMatData), LoadOp::CLEAR, nullptr);
    const auto gbufferPos =
        CreateDefaultColorAttachment(gbufferInfo.GetFormat(GBufferTextureType::Position), LoadOp::CLEAR, nullptr);
    m_mainRenderingData.depthAttachment =
        CreateDefaultDepthAttachment(LoadOp::LOAD, attachmentInfo.depthAttachment.GetTexture());
    m_mainRenderingData.colorAttachments = {gbufferPosition, gbufferNormal, gbuffer3, gbufferPos};

    InitBaseData(attachmentInfo);
    m_indirectCmdBuffer = IndirectDrawCommandBuffer(1000);

    BuildPipelines();
}

void StaticMainMeshPass::BuildPipelines()
{
    ScopedZone("StaticMeshPass::BuildPipelines");

    auto mainVert = Shader("Shaders/SimpleForward.vert.spv", "main");
    auto mainFrag = Shader("Shaders/SimpleForward.frag.spv", "main");

    PipelineInfo info{};
    // info.descriptorSetLayout.pipelineSpecificDescriptors.emplace_back();
    info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
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

    m_indirectCmdBuffer.EmptyCmds();
    u32 instanceOffset = 0;
    stltype::vector<u32> instanceDataIndices;
    instanceDataIndices.reserve(meshes.size());
    for (const auto& mesh : meshes)
    {
        if (mesh.meshData.IsDebugMesh())
            continue;
        const auto& meshHandle = mesh.meshData.meshResourceHandle;

        m_indirectCmdBuffer.AddIndexedDrawCmd(meshHandle.indexCount,
                                              1, // TODO: instanced rendering
                                              meshHandle.indexBufferOffset,
                                              meshHandle.vertBufferOffset,
                                              instanceOffset);
        instanceDataIndices.emplace_back(mesh.meshData.instanceDataIdx);
        ++instanceOffset;
    }
    RebuildPerObjectBuffer(instanceDataIndices);
    m_indirectCmdBuffer.FillCmds();
    // m_needsBufferSync = true;
}

void RenderPasses::StaticMainMeshPass::Render(const MainPassData& data,
                                              FrameRendererContext& ctx,
                                              CommandBuffer* pCmdBuffer)
{
    ScopedZone("StaticMeshPass::Render");

    const auto currentFrame = ctx.imageIdx;
    UpdateContextForFrame(currentFrame);
    const auto& passCtx = m_perObjectFrameContexts[currentFrame];

    ColorAttachment gbufferPosition = m_mainRenderingData.colorAttachments[0];
    ColorAttachment gbufferNormal = m_mainRenderingData.colorAttachments[1];
    ColorAttachment gbuffer3 = m_mainRenderingData.colorAttachments[2];
    ColorAttachment gbufferPos = m_mainRenderingData.colorAttachments[3];
    gbufferPosition.SetTexture(data.pGbuffer->Get(GBufferTextureType::GBufferAlbedo));
    gbufferNormal.SetTexture(data.pGbuffer->Get(GBufferTextureType::GBufferNormal));
    gbuffer3.SetTexture(data.pGbuffer->Get(GBufferTextureType::TexCoordMatData));
    gbufferPos.SetTexture(data.pGbuffer->Get(GBufferTextureType::Position));

    stltype::vector<ColorAttachment> colorAttachments = {gbufferPosition, gbufferNormal, gbuffer3, gbufferPos};

    const auto ex = ctx.pCurrentSwapchainTexture->GetInfo().extents;
    const DirectX::XMINT2 extents(ex.x, ex.y);

    BeginRenderingCmd cmdBegin{&m_mainPSO, colorAttachments, &m_mainRenderingData.depthAttachment};
    cmdBegin.extents = extents;
    cmdBegin.viewport = data.mainView.viewport;

    GenericIndirectDrawCmd cmd{&m_mainPSO, m_indirectCmdBuffer};
    cmd.drawCount = m_indirectCmdBuffer.GetDrawCmdNum();

    auto& sceneGeometryBuffers = data.pResourceManager->GetSceneGeometryBuffers();
    if (sceneGeometryBuffers.GetVertexBuffer().GetRef() == VK_NULL_HANDLE ||
        sceneGeometryBuffers.GetIndexBuffer().GetRef() == VK_NULL_HANDLE)
    {
        return;
    }

    if (data.bufferDescriptors.empty())
        cmd.descriptorSets = {g_pTexManager->GetBindlessDescriptorSet()};
    else
    {
        const auto transformSSBOSet = data.bufferDescriptors.at(UBO::DescriptorContentsType::GlobalInstanceData);
        const auto texArraySet = data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessTextureArray);
        cmd.descriptorSets = {
            texArraySet, data.mainView.descriptorSet, transformSSBOSet, passCtx.m_perObjectDescriptor};
    }
    cmdBegin.drawCmdBuffer = &m_indirectCmdBuffer;
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
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalArrayTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, 1));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TransformSSBO, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GlobalObjectDataSSBOs, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::InstanceDataSSBO, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PerPassObjectSSBO, 3));
}

bool StaticMainMeshPass::WantsToRender() const
{
    return NeedToRender(m_indirectCmdBuffer);
}
} // namespace RenderPasses
