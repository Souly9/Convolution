#include "DepthPrePass.h"
#include "../Utils/RenderPassUtils.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"

namespace RenderPasses
{
DepthPrePass::DepthPrePass() : GenericGeometryPass("DepthPrePass")
{
    SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates::Complete);
    CreateSharedDescriptorLayout();
}

void DepthPrePass::BuildPipelines()
{
    ScopedZone("DepthPrePass::BuildPipelines");

    auto mainVert = Shader("Shaders/Dummy.vert.spv", "main");
    auto mainFrag = Shader("Shaders/Dummy.frag.spv", "main");

    PipelineInfo info{};
    info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
    info.attachmentInfos =
        CreateAttachmentInfo({m_mainRenderingData.colorAttachments}, m_mainRenderingData.depthAttachment);
    m_mainPSO = PSO(
        ShaderCollection{&mainVert, &mainFrag}, PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions}, info);
}

void DepthPrePass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
{
    ScopedZone("DepthPrePass::Init");

    m_mainRenderingData.depthAttachment =
        CreateDefaultDepthAttachment(LoadOp::CLEAR, attachmentInfo.depthAttachment.GetTexture());

    InitBaseData(attachmentInfo);
    m_indirectCmdBuffer = IndirectDrawCommandBuffer(1000);

    BuildPipelines();
}

void DepthPrePass::BuildBuffers()
{
}

void DepthPrePass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                       FrameRendererContext& previousFrameCtx,
                                       u32 thisFrameNum)
{
    ScopedZone("DepthPrePass::Rebuild");

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
}

void DepthPrePass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    ScopedZone("DepthPrePass::Render");

    const auto currentFrame = ctx.imageIdx;
    UpdateContextForFrame(currentFrame);
    const auto& passCtx = m_perObjectFrameContexts[currentFrame];

    const auto ex = m_mainRenderingData.depthAttachment.GetTexture()->GetInfo().extents;
    const DirectX::XMINT2 extents(ex.x, ex.y);

    stltype::vector<ColorAttachment> colorAttachments;
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

    const auto transformSSBOSet = data.bufferDescriptors.at(UBO::DescriptorContentsType::GlobalInstanceData);
    const auto texArraySet = data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessTextureArray);
    cmd.descriptorSets = {texArraySet, ctx.mainViewUBODescriptor, transformSSBOSet, passCtx.m_perObjectDescriptor};

    cmdBegin.drawCmdBuffer = &m_indirectCmdBuffer;

    StartRenderPassProfilingScope(pCmdBuffer);
    pCmdBuffer->RecordCommand(cmdBegin);
    BinRenderDataCmd geomBufferCmd(sceneGeometryBuffers.GetVertexBuffer(), sceneGeometryBuffers.GetIndexBuffer());
    pCmdBuffer->RecordCommand(geomBufferCmd);
    pCmdBuffer->RecordCommand(cmd);
    pCmdBuffer->RecordCommand(EndRenderingCmd{});
    EndRenderPassProfilingScope(pCmdBuffer);
}

void DepthPrePass::CreateSharedDescriptorLayout()
{
    // Don't need them but also doesn't hurt, might help GPU to better cache the first two which are needed basically
    // everywhere else
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalArrayTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, 1));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TransformSSBO, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GlobalObjectDataSSBOs, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::InstanceDataSSBO, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PerPassObjectSSBO, 3));
}

bool DepthPrePass::WantsToRender() const
{
    return NeedToRender(m_indirectCmdBuffer);
}
} // namespace RenderPasses
