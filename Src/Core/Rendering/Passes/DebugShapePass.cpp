#include "DebugShapePass.h"
#include "Core/Rendering/Core/GBuffer.h"
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
    info.depthWriteEnable = false;
    info.attachmentInfos.colorAttachments = { TexFormat::R16G16B16A16_FLOAT };
    info.attachmentInfos.depthAttachmentFormat = TexFormat::D32_SFLOAT;
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
        return;
    }

    m_indirectCmdBuffers[m_currentFrameIdx].EmptyCmds();
    m_indirectCmdBuffersWireframe[m_currentFrameIdx].EmptyCmds();
    u32 instanceOffset = 0;
    stltype::vector<u32> instanceDataIndices;
    instanceDataIndices.reserve(meshes.size());
    for (const auto& mesh : meshes)
    {
        if (!mesh.meshData.IsDebugMesh())
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
    RebuildPerObjectBuffer(instanceDataIndices, m_currentFrameIdx);
    m_indirectCmdBuffers[m_currentFrameIdx].FillCmds();
    m_indirectCmdBuffersWireframe[m_currentFrameIdx].FillCmds();
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
    builder.DeclareContexts<
        PassCtx::Bindless,
        PassCtx::View,
        PassCtx::GlobalInstance>();

    builder.WriteGBuffer(RGResourceID::GBufferDebug, LoadOp::LOAD);
    builder.ReadDepth(RGResourceID::MainDepth);
    builder.SetHasSideEffects();
}

void DebugShapePass::RenderWithGraph(const MainPassData& data,
                                     const FrameRendererContext& ctx,
                                     const RGExecutionContext& execCtx)
{
    const auto currentFrame = execCtx.GetFrameIndex();
    const auto& passCtx = m_perObjectFrameContexts[currentFrame];

    RenderAttachmentInfo colorAttachment = execCtx.GetColorAttachment(RGResourceID::GBufferDebug, LoadOp::LOAD);
    RenderAttachmentInfo depthAttachment = execCtx.GetReadOnlyDepthAttachment(RGResourceID::MainDepth);

    const DirectX::XMINT2 extents(execCtx.GetRenderResolution().x, execCtx.GetRenderResolution().y);

    auto& sceneGeometryBuffers = data.pResourceManager->GetDebugGeometryBuffers();
    if (!sceneGeometryBuffers.GetVertexBuffer().IsCreated() || !sceneGeometryBuffers.GetIndexBuffer().IsCreated())
    {
        return;
    }
    BinRenderDataCmd geomBufferCmd(sceneGeometryBuffers.GetVertexBuffer(), sceneGeometryBuffers.GetIndexBuffer());

    StartRenderPassProfilingScope(execCtx.pCmdBuffer);

    auto descriptorSets = execCtx.GetDescriptors();
    descriptorSets.push_back(passCtx.m_perObjectDescriptor);

    auto& opaqueBuffer = m_indirectCmdBuffers[currentFrame];
    auto& wireframeBuffer = m_indirectCmdBuffersWireframe[currentFrame];

    if (opaqueBuffer.GetDrawCmdNum() > 0)
    {
        GenericIndirectDrawCmd cmd{&m_solidDebugObjectsPSO, opaqueBuffer};
        cmd.descriptorSets = descriptorSets;
        cmd.drawCount = opaqueBuffer.GetDrawCmdNum();

        BeginRenderingCmd cmdBegin{&m_solidDebugObjectsPSO,
                                   {colorAttachment},
                                   depthAttachment};
        cmdBegin.extents = extents;
        cmdBegin.viewport = data.mainView.viewport;
        execCtx.pCmdBuffer->RecordCommand(cmdBegin);
        execCtx.pCmdBuffer->RecordCommand(geomBufferCmd);
        execCtx.pCmdBuffer->RecordCommand(cmd);
        execCtx.pCmdBuffer->RecordCommand(EndRenderingCmd{});
    }
    if (wireframeBuffer.GetDrawCmdNum() > 0)
    {
        GenericIndirectDrawCmd cmd{&m_wireframeDebugObjectsPSO, wireframeBuffer};
        cmd.descriptorSets = descriptorSets;
        cmd.drawCount = wireframeBuffer.GetDrawCmdNum();

        BeginRenderingCmd cmdBegin{&m_wireframeDebugObjectsPSO,
                                   {colorAttachment},
                                   depthAttachment};
        cmdBegin.extents = extents;
        cmdBegin.viewport = data.mainView.viewport;

        execCtx.pCmdBuffer->RecordCommand(cmdBegin);
        execCtx.pCmdBuffer->RecordCommand(geomBufferCmd);
        execCtx.pCmdBuffer->RecordCommand(cmd);
        execCtx.pCmdBuffer->RecordCommand(EndRenderingCmd{});
    }
    EndRenderPassProfilingScope(execCtx.pCmdBuffer);
}

bool DebugShapePass::WantsToRender() const
{
    return false; // NeedToRender(m_indirectCmdBuffers[m_currentFrameIdx]) ||
                  // NeedToRender(m_indirectCmdBuffersWireframe[m_currentFrameIdx]);
}
