#include "StaticMeshPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Core/CommandBuffer.h"
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

void StaticMainMeshPass::Init(const SharedResourceManager& resourceManager)
{
    ScopedZone("StaticMeshPass::Init");

    RecreateResolutionDependentResources(resourceManager);
    BuildPipelines();

    m_indirectCmdBuffers.resize(SWAPCHAIN_IMAGES);
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
        m_indirectCmdBuffers[i].Init(1000000);
    BuildPipelines();
}

void StaticMainMeshPass::RecreateResolutionDependentResources(const SharedResourceManager& resourceManager)
{
    ScopedZone("StaticMeshPass::RecreateResolutionDependentResources");
    InitBaseData();
}

void StaticMainMeshPass::BuildPipelines()
{
    ScopedZone("StaticMeshPass::BuildPipelines");

    auto mainVert = Shader("Shaders/GBufferPass.vert.spv", "main");
    auto mainFrag = Shader("Shaders/GBufferPass.frag.spv", "main");

    PipelineInfo info{};
    info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
    info.depthWriteEnable = false;
    info.depthCompareOp = DepthCompareOp::GREATER_OR_EQUAL;
    info.attachmentInfos.colorAttachments = {
        TexFormat::R16G16B16A16_FLOAT, // GBufferAlbedo
        TexFormat::R16G16B16A16_FLOAT, // GBufferNormal
        TexFormat::R16G16B16A16_FLOAT, // GBufferMaterial
        TexFormat::R32G32_FLOAT,       // GBufferVelocity
        TexFormat::R8_UNORM,           // GBufferRoughness
        TexFormat::R32_UINT            // GBufferEntityID
    };
    info.attachmentInfos.depthAttachmentFormat = TexFormat::D32_SFLOAT;
    m_mainPSO = PSO(
        ShaderCollection{&mainVert, &mainFrag}, PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions}, info);
}

void StaticMainMeshPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                             FrameRendererContext& previousFrameCtx,
                                             u32 thisFrameNum)
{
    ScopedZone("StaticMeshPass::Rebuild");

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
    RebuildPerObjectBuffer(instanceDataIndices, thisFrameNum);
    cmdBuf.FillCmds();
}

void StaticMainMeshPass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.DeclareContexts<
        PassCtx::Bindless,
        PassCtx::View,
        PassCtx::GlobalInstance>();

    builder.WriteGBuffer(RGResourceID::GBufferAlbedo);
    builder.WriteGBuffer(RGResourceID::GBufferNormal);
    builder.WriteGBuffer(RGResourceID::GBufferMaterial);
    builder.WriteGBuffer(RGResourceID::GBufferVelocity);
    builder.WriteGBuffer(RGResourceID::GBufferRoughness);
    builder.WriteGBuffer(RGResourceID::GBufferEntityID);
    builder.ReadDepth(RGResourceID::MainDepth);
    builder.SetHasSideEffects();
}

void StaticMainMeshPass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("StaticMeshPass::Render");

    const auto currentFrame = execCtx.GetFrameIndex();
    const auto& passCtx = m_perObjectFrameContexts[currentFrame];

    stltype::vector<RenderAttachmentInfo> colorAttachments = {
        execCtx.GetColorAttachment(RGResourceID::GBufferAlbedo),
        execCtx.GetColorAttachment(RGResourceID::GBufferNormal),
        execCtx.GetColorAttachment(RGResourceID::GBufferMaterial),
        execCtx.GetColorAttachment(RGResourceID::GBufferVelocity),
        execCtx.GetColorAttachment(RGResourceID::GBufferRoughness),
        execCtx.GetColorAttachment(RGResourceID::GBufferEntityID)
    };
    RenderAttachmentInfo depthAttachment = execCtx.GetReadOnlyDepthAttachment(RGResourceID::MainDepth);

    const DirectX::XMINT2 extents(execCtx.GetRenderResolution().x, execCtx.GetRenderResolution().y);

    BeginRenderingCmd cmdBegin{&m_mainPSO, colorAttachments, depthAttachment};
    cmdBegin.extents = extents;
    cmdBegin.viewport = data.mainView.viewport;

    auto& cmdBuf = m_indirectCmdBuffers[currentFrame];
    GenericIndirectDrawCmd cmd{&m_mainPSO, cmdBuf};
    cmd.drawCount = cmdBuf.GetDrawCmdNum();

    auto& sceneGeometryBuffers = data.pResourceManager->GetSceneGeometryBuffers();
    if (!sceneGeometryBuffers.GetVertexBuffer().IsCreated() ||
        !sceneGeometryBuffers.GetIndexBuffer().IsCreated())
    {
        return;
    }

    cmd.descriptorSets = execCtx.GetDescriptors();
    cmd.descriptorSets.push_back(passCtx.m_perObjectDescriptor);

    cmdBegin.drawCmdBuffer = &cmdBuf;
    StartRenderPassProfilingScope(execCtx.pCmdBuffer);
    execCtx.pCmdBuffer->RecordCommand(cmdBegin);
    BinRenderDataCmd geomBufferCmd(sceneGeometryBuffers.GetVertexBuffer(), sceneGeometryBuffers.GetIndexBuffer());
    execCtx.pCmdBuffer->RecordCommand(geomBufferCmd);
    execCtx.pCmdBuffer->RecordCommand(cmd);
    execCtx.pCmdBuffer->RecordCommand(EndRenderingCmd{});
    EndRenderPassProfilingScope(execCtx.pCmdBuffer);
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
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        if (NeedToRender(m_indirectCmdBuffers[i]))
            return true;
    }
    return false;
}
