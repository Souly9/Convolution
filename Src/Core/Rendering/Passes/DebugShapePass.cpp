#include "DebugShapePass.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Utils/RenderPassUtils.h"

using namespace RenderPasses;

DebugShapePass::DebugShapePass() : GenericGeometryPass("DebugShapePass")
{
    SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates::Complete);
    m_indirectCmdBuffersWireframe.resize(SWAPCHAIN_IMAGES);
    CreateSharedDescriptorLayout();
}

void DebugShapePass::BuildBuffers()
{
}

void DebugShapePass::Init(const SharedResourceManager& resourceManager)
{
    ScopedZone("DebugShapePass::Init");

    RecreateResolutionDependentResources(resourceManager);
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        m_indirectCmdBuffersWireframe[i].Init(250000);
        m_indirectCmdBuffers[i].Init(250000);
    }
    BuildPipelines();
}

void DebugShapePass::RecreateResolutionDependentResources(const SharedResourceManager& resourceManager)
{
    ScopedZone("DebugShapePass::RecreateResolutionDependentResources");
    InitBaseData();
}

void DebugShapePass::BuildPipelines()
{
    ScopedZone("DebugShapePass::BuildPipelines");

    auto mainVert = Shader("Shaders/Debug.vert.spv", "main");
    auto mainFrag = Shader("Shaders/Debug.frag.spv", "main");
    PipelineInfo info{};
    info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
    info.attachmentInfos.colorAttachments = {TexFormat::R8G8B8A8_UNORM};
    info.attachmentInfos.depthAttachmentFormat = DEPTH_BUFFER_FORMAT;
    // Debug shapes aren't in the depth prepass, so they test instead of matching it
    info.depthCompareOp = kDepthWriteCompareOp;
    m_solidDebugObjectsPSO = PSO(
        ShaderCollection{&mainVert, &mainFrag}, PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions}, info);

    // Selection outlines stay visible through whatever covers the selected mesh
    auto wireframeInfo = info;
    wireframeInfo.rasterizerInfo.polyMode = PolygonMode::Line;
    wireframeInfo.depthCompareOp = DepthCompareOp::ALWAYS;
    m_wireframeDebugObjectsPSO = PSO(ShaderCollection{&mainVert, &mainFrag},
                                     PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions},
                                     wireframeInfo);
}

void DebugShapePass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                         FrameRendererContext& previousFrameCtx,
                                         u32 thisFrameNum)
{
    ScopedZone("DebugShapePass::Rebuild");
    auto& solidCmds = m_indirectCmdBuffers[thisFrameNum];
    auto& wireframeCmds = m_indirectCmdBuffersWireframe[thisFrameNum];
    solidCmds.EmptyCmds();
    wireframeCmds.EmptyCmds();

    // Debug meshes are drawn solid from the debug geometry, selected scene meshes as wireframe from the scene geometry
    stltype::vector<u32> instanceDataIndices;
    for (const auto& mesh : meshes)
    {
        const bool isDebug = mesh.meshData.IsDebugMesh();
        if (!isDebug && !mesh.meshData.IsDebugWireframeMesh())
            continue;
        const auto& meshHandle = mesh.meshData.meshResourceHandle;
        auto& cmds = isDebug ? solidCmds : wireframeCmds;
        cmds.AddIndexedDrawCmd(meshHandle.indexCount,
                               1,
                               meshHandle.indexBufferOffset,
                               meshHandle.vertBufferOffset,
                               static_cast<u32>(instanceDataIndices.size()));
        instanceDataIndices.push_back(mesh.meshData.instanceDataIdx);
    }
    if (instanceDataIndices.empty())
        return;

    RebuildPerObjectBuffer(instanceDataIndices, thisFrameNum);
    solidCmds.FillCmds();
    wireframeCmds.FillCmds();
}

void DebugShapePass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset(DescriptorPresets::Bindless());
    AppendLayoutPreset(DescriptorPresets::View());
    AppendLayoutPreset(DescriptorPresets::GlobalInstanceData());
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PerPassObjectSSBO, 3));
}

#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"

void DebugShapePass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.DeclareContexts<PassCtx::Bindless, PassCtx::View, PassCtx::GlobalInstance>();

    builder.WriteGBuffer(RGResourceID::DebugOverlay, LoadOp::CLEAR);
    builder.ReadDepth(RGResourceID::MainDepth);
}

void DebugShapePass::RenderWithGraph(const MainPassData& data,
                                     const FrameRendererContext& ctx,
                                     const RGExecutionContext& execCtx)
{
    const u32 frameIdx = execCtx.GetFrameIndex();
    const auto& solidCmds = m_indirectCmdBuffers[frameIdx];
    const auto& wireframeCmds = m_indirectCmdBuffersWireframe[frameIdx];
    const RenderAttachmentInfo depthAttachment = execCtx.GetReadOnlyDepthAttachment(RGResourceID::MainDepth);
    const DirectX::XMINT2 extents(execCtx.GetRenderResolution().x, execCtx.GetRenderResolution().y);

    auto descriptorSets = execCtx.GetDescriptors();
    descriptorSets.push_back(m_perObjectFrameContexts[frameIdx].m_perObjectDescriptor);

    StartRenderPassProfilingScope(execCtx.pCmdBuffer);

    // Begins even without solid draws so the overlay is cleared every frame the composite reads it
    BeginRenderingCmd solidBegin{&m_solidDebugObjectsPSO,
                                 {execCtx.GetColorAttachment(RGResourceID::DebugOverlay, LoadOp::CLEAR)},
                                 depthAttachment};
    solidBegin.extents = extents;
    solidBegin.viewport = data.mainView.viewport;
    execCtx.pCmdBuffer->RecordCommand(solidBegin);
    if (solidCmds.GetDrawCmdNum() > 0)
    {
        auto& debugGeometry = data.pResourceManager->GetDebugGeometryBuffers();
        const BinRenderDataCmd geometryCmd(debugGeometry.GetVertexBuffer(), debugGeometry.GetIndexBuffer());
        GenericIndirectDrawCmd cmd{&m_solidDebugObjectsPSO, solidCmds};
        cmd.descriptorSets = descriptorSets;
        cmd.drawCount = solidCmds.GetDrawCmdNum();
        execCtx.pCmdBuffer->RecordCommand(geometryCmd);
        execCtx.pCmdBuffer->RecordCommand(cmd);
    }
    execCtx.pCmdBuffer->RecordCommand(EndRenderingCmd{});

    if (wireframeCmds.GetDrawCmdNum() > 0)
    {
        auto& sceneGeometry = data.pResourceManager->GetSceneGeometryBuffers();
        const BinRenderDataCmd geometryCmd(sceneGeometry.GetVertexBuffer(), sceneGeometry.GetIndexBuffer());
        GenericIndirectDrawCmd cmd{&m_wireframeDebugObjectsPSO, wireframeCmds};
        cmd.descriptorSets = descriptorSets;
        cmd.drawCount = wireframeCmds.GetDrawCmdNum();

        BeginRenderingCmd wireframeBegin{&m_wireframeDebugObjectsPSO,
                                         {execCtx.GetColorAttachment(RGResourceID::DebugOverlay, LoadOp::LOAD)},
                                         depthAttachment};
        wireframeBegin.extents = extents;
        wireframeBegin.viewport = data.mainView.viewport;
        // Separate rendering instances aren't ordered, the graph only orders across nodes
        execCtx.pCmdBuffer->RecordCommand(
            GlobalBarrierCmd(SyncStages::COLOR_ATTACHMENT_OUTPUT,
                             SyncStages::COLOR_ATTACHMENT_OUTPUT,
                             AccessFlags::COLOR_ATTACHMENT_WRITE,
                             AccessFlags::COLOR_ATTACHMENT_READ | AccessFlags::COLOR_ATTACHMENT_WRITE));
        execCtx.pCmdBuffer->RecordCommand(wireframeBegin);
        execCtx.pCmdBuffer->RecordCommand(geometryCmd);
        execCtx.pCmdBuffer->RecordCommand(cmd);
        execCtx.pCmdBuffer->RecordCommand(EndRenderingCmd{});
    }
    EndRenderPassProfilingScope(execCtx.pCmdBuffer);
}

bool DebugShapePass::WantsToRender() const
{
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        if (NeedToRender(m_indirectCmdBuffers[i]) || NeedToRender(m_indirectCmdBuffersWireframe[i]))
            return true;
    }
    return false;
}
