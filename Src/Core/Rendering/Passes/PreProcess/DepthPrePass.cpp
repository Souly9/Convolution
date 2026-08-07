#include "DepthPrePass.h"
#include "../Utils/RenderPassUtils.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"

using namespace RenderPasses;
DepthPrePass::DepthPrePass() : GenericGeometryPass("DepthPrePass")
{
    SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates::Complete);
    CreateSharedDescriptorLayout();
}

void DepthPrePass::BuildPipelines()
{
    ScopedZone("DepthPrePass::BuildPipelines");

    auto mainVert = Shader("Shaders/Dummy.vert.spv", "main");
    auto mainFrag = Shader("Shaders/DepthPass.frag.spv", "main");

    PipelineInfo info{};
    info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
    info.attachmentInfos =
        CreateAttachmentInfo({m_mainRenderingData.colorAttachments}, m_mainRenderingData.depthAttachment);
    info.depthCompareOp = DepthCompareOp::GREATER_OR_EQUAL;
    info.depthWriteEnable = true;
    // info.rasterizerInfo.cullmode = CullMode::BACK;

    m_mainPSO = PSO(
        ShaderCollection{&mainVert, &mainFrag}, PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions}, info);
}

void DepthPrePass::Init(const SharedResourceManager& resourceManager)
{
    ScopedZone("DepthPrePass::Init");

    RecreateResolutionDependentResources(resourceManager);
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
        m_indirectCmdBuffers[i].Init(1000000);
    BuildPipelines();
}

void DepthPrePass::RecreateResolutionDependentResources(const SharedResourceManager& resourceManager)
{
    ScopedZone("DepthPrePass::RecreateResolutionDependentResources");

    m_mainRenderingData.depthAttachment =
        CreateDefaultDepthAttachment(LoadOp::CLEAR, nullptr);

    InitBaseData();
}

void DepthPrePass::BuildBuffers()
{
}

void DepthPrePass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                       FrameRendererContext& previousFrameCtx,
                                       u32 thisFrameNum)
{
    ScopedZone("DepthPrePass::Rebuild");

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

void DepthPrePass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset(DescriptorPresets::Bindless(true));
    AppendLayoutPreset(DescriptorPresets::View());
    AppendLayoutPreset(DescriptorPresets::GlobalInstanceData());
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PerPassObjectSSBO, 3));
}

#include "Core/Rendering/Core/RenderGraph/PassContext.h"
#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"

void DepthPrePass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.DeclareContexts<
        PassCtx::BindlessWithImages,
        PassCtx::View,
        PassCtx::GlobalInstance>();

    auto mainDepth = builder.WriteDepthAttachment(RGResourceID::MainDepth, LoadOp::CLEAR, StoreOp::STORE);
    builder.SetHasSideEffects();
}

void DepthPrePass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("DepthPrePass::RenderWithGraph");
    StartRenderPassProfilingScope(execCtx.pCmdBuffer);

    const auto currentFrame = ctx.currentFrame;
    UpdateContextForFrame(currentFrame);
    const auto& passCtx = m_perObjectFrameContexts[currentFrame];

    m_mainRenderingData.depthAttachment.SetTexture(execCtx.GetTexture(RGResourceID::MainDepth));
    const DirectX::XMINT2 extents(data.renderState.renderResolution.x, data.renderState.renderResolution.y);
    stltype::vector<ColorAttachment> colorAttachments;
    BeginRenderingCmd cmdBegin{&m_mainPSO,
                               ToRenderAttachmentInfos(colorAttachments),
                               ToRenderAttachmentInfo(m_mainRenderingData.depthAttachment)};
    cmdBegin.extents = extents;
    cmdBegin.viewport = data.mainView.viewport;

    auto& cmdBuf = m_indirectCmdBuffers[currentFrame];
    GenericIndirectDrawCmd cmd{&m_mainPSO, cmdBuf};
    cmd.drawCount = cmdBuf.GetDrawCmdNum();

    auto& sceneGeometryBuffers = data.pResourceManager->GetSceneGeometryBuffers();
    if (!sceneGeometryBuffers.GetVertexBuffer().IsCreated() ||
        !sceneGeometryBuffers.GetIndexBuffer().IsCreated())
    {
        EndRenderPassProfilingScope(execCtx.pCmdBuffer);
        return;
    }

    cmd.descriptorSets = execCtx.GetDescriptors();
    cmd.descriptorSets.push_back(passCtx.m_perObjectDescriptor);

    cmdBegin.drawCmdBuffer = &cmdBuf;

    execCtx.pCmdBuffer->RecordCommand(cmdBegin);
    BinRenderDataCmd geomBufferCmd(sceneGeometryBuffers.GetVertexBuffer(), sceneGeometryBuffers.GetIndexBuffer());
    execCtx.pCmdBuffer->RecordCommand(geomBufferCmd);
    execCtx.pCmdBuffer->RecordCommand(cmd);
    execCtx.pCmdBuffer->RecordCommand(EndRenderingCmd{});

    EndRenderPassProfilingScope(execCtx.pCmdBuffer);
}

bool DepthPrePass::WantsToRender() const
{
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        if (NeedToRender(m_indirectCmdBuffers[i]))
            return true;
    }
    return false;
}

