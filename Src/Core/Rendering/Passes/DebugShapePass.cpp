#include "DebugShapePass.h"
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

void DebugShapePass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
{
    ScopedZone("DebugShapePass::Init");

    RecreateResolutionDependentResources(attachmentInfo, resourceManager);
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        m_indirectCmdBuffersWireframe[i].Init(250000);
        m_indirectCmdBuffers[i].Init(250000);
    }
    BuildPipelines();
}

void DebugShapePass::RecreateResolutionDependentResources(RendererAttachmentInfo& attachmentInfo,
                                                          const SharedResourceManager& resourceManager)
{
    ScopedZone("DebugShapePass::RecreateResolutionDependentResources");

    const auto& gbufferInfo = attachmentInfo.gbuffer;

    const auto debugAttachment =
        CreateDefaultColorAttachment(gbufferInfo.GetFormat(GBufferTextureType::GBufferDebug), LoadOp::CLEAR, nullptr);

    m_mainRenderingData.depthAttachment =
        CreateReadOnlyDepthAttachment(LoadOp::LOAD, attachmentInfo.depthAttachment.GetTexture());
    m_mainRenderingData.colorAttachments = {debugAttachment};

    InitBaseData(attachmentInfo);
}

void DebugShapePass::BuildPipelines()
{
    ScopedZone("DebugShapePass::BuildPipelines");

    auto mainVert = Shader("Shaders/Debug.vert.spv", "main");
    auto mainFrag = Shader("Shaders/Debug.frag.spv", "main");
    PipelineInfo info{};
    // info.descriptorSetLayout.pipelineSpecificDescriptors.emplace_back();
    info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
    info.depthWriteEnable = false;
    info.attachmentInfos =
        CreateAttachmentInfo(m_mainRenderingData.colorAttachments, m_mainRenderingData.depthAttachment);
    m_solidDebugObjectsPSO = PSO(
        ShaderCollection{&mainVert, &mainFrag}, PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions}, info);

    auto wireFrameInfo = info;
    wireFrameInfo.topology = Topology::Lines;
    m_wireframeDebugObjectsPSO = PSO(ShaderCollection{&mainVert, &mainFrag},
                                     PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions},
                                     wireFrameInfo);
}

void DebugShapePass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                         FrameRendererContext& previousFrameCtx,
                                         u32 thisFrameNum)
{
    ScopedZone("DebugShapePass::Rebuild");
    m_currentFrameIdx = thisFrameNum % SWAPCHAIN_IMAGES;
    m_instancedMeshInfoMap.clear();
    bool areAnyDebug = false;
    m_indirectCmdBuffers[m_currentFrameIdx].EmptyCmds();
    m_indirectCmdBuffersWireframe[m_currentFrameIdx].EmptyCmds();
    for (const auto& meshData : meshes)
    {
        if (meshData.meshData.IsDebugMesh())
        {
            areAnyDebug = true;
            break;
        }
    }
    if (areAnyDebug == false)
    {
        // m_mainRenderingData.ClearBuffers();
        return;
    }

    m_indirectCmdBuffers[m_currentFrameIdx].EmptyCmds();
    m_indirectCmdBuffersWireframe[m_currentFrameIdx].EmptyCmds();
    u32 instanceOffset = 0;
    stltype::vector<u32> instanceDataIndices;
    instanceDataIndices.reserve(meshes.size());
    for (const auto& mesh : meshes)
    {
        if (mesh.meshData.IsDebugMesh() == false)
            continue;
        const auto& meshHandle = mesh.meshData.meshResourceHandle;

        if (mesh.meshData.IsDebugWireframeMesh())
        {
            m_indirectCmdBuffersWireframe[m_currentFrameIdx].AddIndexedDrawCmd(meshHandle.indexCount,
                                                                               1, // TODO: instanced rendering
                                                                               meshHandle.indexBufferOffset,
                                                                               meshHandle.vertBufferOffset,
                                                                               instanceOffset);
        }
        else
        {
            m_indirectCmdBuffers[m_currentFrameIdx].AddIndexedDrawCmd(meshHandle.indexCount,
                                                                      1, // TODO: instanced rendering
                                                                      meshHandle.indexBufferOffset,
                                                                      meshHandle.vertBufferOffset,
                                                                      instanceOffset);
        }
        instanceDataIndices.emplace_back(mesh.meshData.instanceDataIdx);
        ++instanceOffset;
    }
    RebuildPerObjectBuffer(instanceDataIndices);
    m_indirectCmdBuffers[m_currentFrameIdx].FillCmds();
    m_indirectCmdBuffersWireframe[m_currentFrameIdx].FillCmds();
}

void DebugShapePass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    ScopedZone("DebugShapePass::Render");
    const auto currentFrame = ctx.currentFrame;
    UpdateContextForFrame(currentFrame);
    const auto& passCtx = m_perObjectFrameContexts[currentFrame];

    ColorAttachment debugAttachment = m_mainRenderingData.colorAttachments[0];
    debugAttachment.SetTexture(data.pGbuffer->Get(GBufferTextureType::GBufferDebug));

    stltype::vector<ColorAttachment> colorAttachments = {debugAttachment};
    const DirectX::XMINT2 extents(data.renderState.renderResolution.x, data.renderState.renderResolution.y);

    auto& sceneGeometryBuffers = data.pResourceManager->GetDebugGeometryBuffers();
    if (sceneGeometryBuffers.GetVertexBuffer().GetRef() == VK_NULL_HANDLE ||
        sceneGeometryBuffers.GetIndexBuffer().GetRef() == VK_NULL_HANDLE)
    {
        return;
    }
    BinRenderDataCmd geomBufferCmd(sceneGeometryBuffers.GetVertexBuffer(), sceneGeometryBuffers.GetIndexBuffer());

    if (data.bufferDescriptors.empty() == false)
    {
        StartRenderPassProfilingScope(pCmdBuffer);

        const auto transformSSBOSet = data.bufferDescriptors.at(UBO::DescriptorContentsType::GlobalInstanceData);
        const auto texArraySet = data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessTextureArray);

        auto& opaqueBuffer = m_indirectCmdBuffers[m_currentFrameIdx];
        auto& wireframeBuffer = m_indirectCmdBuffersWireframe[m_currentFrameIdx];

        if (opaqueBuffer.GetDrawCmdNum() > 0)
        {
            GenericIndirectDrawCmd cmd{&m_solidDebugObjectsPSO, opaqueBuffer};
            cmd.descriptorSets = {
                texArraySet, data.mainView.descriptorSet, transformSSBOSet, passCtx.m_perObjectDescriptor};
            cmd.drawCount = opaqueBuffer.GetDrawCmdNum();

            m_mainRenderingData.depthAttachment.SetTexture(data.pMainDepthTexture);
            BeginRenderingCmd cmdBegin{&m_solidDebugObjectsPSO,
                                       ToRenderAttachmentInfos(colorAttachments),
                                       ToRenderAttachmentInfo(m_mainRenderingData.depthAttachment)};
            cmdBegin.extents = extents;
            cmdBegin.viewport = data.mainView.viewport;
            pCmdBuffer->RecordCommand(cmdBegin);
            pCmdBuffer->RecordCommand(geomBufferCmd);
            pCmdBuffer->RecordCommand(cmd);
            pCmdBuffer->RecordCommand(EndRenderingCmd{});
        }
        if (wireframeBuffer.GetDrawCmdNum() > 0)
        {
            GenericIndirectDrawCmd cmd{&m_wireframeDebugObjectsPSO, wireframeBuffer};
            cmd.descriptorSets = {
                texArraySet, data.mainView.descriptorSet, transformSSBOSet, passCtx.m_perObjectDescriptor};
            cmd.drawCount = wireframeBuffer.GetDrawCmdNum();

            m_mainRenderingData.depthAttachment.SetTexture(data.pMainDepthTexture);
            BeginRenderingCmd cmdBegin{&m_wireframeDebugObjectsPSO,
                                       ToRenderAttachmentInfos(colorAttachments),
                                       ToRenderAttachmentInfo(m_mainRenderingData.depthAttachment)};
            cmdBegin.extents = extents;
            cmdBegin.viewport = data.mainView.viewport;

            pCmdBuffer->RecordCommand(cmdBegin);
            pCmdBuffer->RecordCommand(geomBufferCmd);
            pCmdBuffer->RecordCommand(cmd);
            pCmdBuffer->RecordCommand(EndRenderingCmd{});
        }
        EndRenderPassProfilingScope(pCmdBuffer);
    }
}

void DebugShapePass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset(DescriptorPresets::Bindless());
    AppendLayoutPreset(DescriptorPresets::View());
    AppendLayoutPreset(DescriptorPresets::GlobalInstanceData());
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PerPassObjectSSBO, 3));
}

bool DebugShapePass::WantsToRender() const
{
    return false; // NeedToRender(m_indirectCmdBuffers[m_currentFrameIdx]) ||
                  // NeedToRender(m_indirectCmdBuffersWireframe[m_currentFrameIdx]);
}
