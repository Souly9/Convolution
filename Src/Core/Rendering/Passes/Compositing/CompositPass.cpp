#include "CompositPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/View.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Global/Profiling.h"
#include "Core/Rendering/Core/AntiAliasing.h"
#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"

using namespace RenderPasses;

CompositPass::CompositPass() : GenericGeometryPass("CompositPass")
{
    SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates::Complete);
    CreateSharedDescriptorLayout();
}

void CompositPass::Init(const SharedResourceManager& resourceManager)
{
    ScopedZone("CompositPass::Init");

    RecreateResolutionDependentResources(resourceManager);
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
        m_indirectCmdBuffers[i].Init(10);
    BuildPipelines();
}

void CompositPass::RecreateResolutionDependentResources(const SharedResourceManager& resourceManager)
{
    ScopedZone("CompositPass::RecreateResolutionDependentResources");
    InitBaseData();
}

void CompositPass::BuildPipelines()
{
    ScopedZone("CompositPass::BuildPipelines");
    auto mainVert = Shader("Shaders/CompositPass.vert.spv", "main");
    auto mainFrag = Shader("Shaders/CompositPass.frag.spv", "main");

    PipelineInfo info{};
    info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
    info.attachmentInfos.colorAttachments = { g_renderer.GetSwapchainFormat() };
    info.hasDepth = false;
    m_mainPSO = PSO(
        ShaderCollection{&mainVert, &mainFrag}, PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions}, info);

    info.attachmentInfos.colorAttachments = { GetDefaultFormatForRGResourceID(RGResourceID::GBufferPostAAColor) };
    m_postAAPSO = PSO(
        ShaderCollection{&mainVert, &mainFrag}, PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions}, info);
}

void CompositPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                                      FrameRendererContext& previousFrameCtx,
                                                      u32 thisFrameNum)
{
    m_currentFrameIdx = thisFrameNum % SWAPCHAIN_IMAGES;
    auto& cmdBuf = m_indirectCmdBuffers.at(m_currentFrameIdx);
    cmdBuf.EmptyCmds();
    const auto pFullScreenQuadMesh = g_engine.GetMeshManager().GetPrimitiveMesh(MeshManager::PrimitiveType::Quad);
    const auto meshHandle = previousFrameCtx.pResourceManager->GetMeshHandle(pFullScreenQuadMesh);
    cmdBuf.AddIndexedDrawCmd(
        meshHandle.indexCount, 1, meshHandle.indexBufferOffset, meshHandle.vertBufferOffset, 0);
    RebuildPerObjectBuffer({0}, m_currentFrameIdx);
    cmdBuf.FillCmds();
}

void CompositPass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("CompositPass::Render");

    CommandBuffer* pCmdBuffer = execCtx.pCmdBuffer;

    RenderAttachmentInfo swapchainAttachment =
        execCtx.GetColorAttachment(AA::Current().CompositeTarget(), LoadOp::CLEAR, StoreOp::STORE);
    if (!swapchainAttachment.pTexture)
        swapchainAttachment.pTexture = ctx.pCurrentSwapchainTexture;

    const auto ex = swapchainAttachment.pTexture ? swapchainAttachment.pTexture->GetInfo().extents : ctx.pCurrentSwapchainTexture->GetInfo().extents;
    const DirectX::XMINT2 extents(ex.x, ex.y);

    PSO* pPSO = AA::Current().CompositeTarget() == RGResourceID::GBufferPostAAColor ? &m_postAAPSO : &m_mainPSO;
    BeginRenderingCmd cmdBegin{pPSO, {swapchainAttachment}};
    cmdBegin.extents = extents;
    cmdBegin.viewport = RenderViewUtils::CreateViewportFromData(data.renderState.swapchainResolution, ctx.zNear, ctx.zFar);

    auto& sceneGeometryBuffers = data.pResourceManager->GetSceneGeometryBuffers();
    if (!sceneGeometryBuffers.GetVertexBuffer().IsCreated() ||
        !sceneGeometryBuffers.GetIndexBuffer().IsCreated())
    {
        return;
    }
    BinRenderDataCmd geomBufferCmd(sceneGeometryBuffers.GetVertexBuffer(), sceneGeometryBuffers.GetIndexBuffer());

    auto& cmdBuf = m_indirectCmdBuffers[execCtx.GetFrameIndex()];
    GenericIndirectDrawCmd cmd{pPSO, cmdBuf};
    cmd.drawCount = cmdBuf.GetDrawCmdNum();
    cmd.descriptorSets = execCtx.GetDescriptors();

    StartRenderPassProfilingScope(pCmdBuffer);
    pCmdBuffer->RecordCommand(cmdBegin);
    pCmdBuffer->RecordCommand(geomBufferCmd);
    pCmdBuffer->RecordCommand(cmd);
    pCmdBuffer->RecordCommand(EndRenderingCmd{});
    EndRenderPassProfilingScope(pCmdBuffer);
}

void CompositPass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.DeclareContexts<
        PassCtx::Bindless,
        PassCtx::View,
        PassCtx::GlobalInstance,
        PassCtx::GBufferCtx>();

    const auto& appRenderState = g_engine.GetApplicationState().GetCurrentApplicationState().renderState;
    const AA::FrameConfig& aa = AA::Current();
    const RGResourceID compositeInput = AA::CompositeInput(appRenderState, aa);

    // Always read the AA output so TAA history ends the frame in a sampled layout
    builder.ReadTexture(aa.aaOutput, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    if (compositeInput != aa.aaOutput)
        builder.ReadTexture(compositeInput, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::BloomMip0, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    if (appRenderState.debugViewMode == static_cast<s32>(DebugViewMode::MotionVectors))
        builder.ReadTexture(RGResourceID::GBufferVelocity, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    builder.WriteColorAttachment(aa.CompositeTarget(), LoadOp::CLEAR, StoreOp::STORE);
    builder.SetHasSideEffects();
}

#include "Core/Rendering/Core/RenderGraph/RGExecutionContext.h"

void CompositPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset(DescriptorPresets::Bindless(false));
    AppendLayoutPreset(DescriptorPresets::View());
    AppendLayoutPreset(DescriptorPresets::GlobalInstanceData());
    AppendLayoutPreset(DescriptorPresets::GBuffer());
}

bool CompositPass::WantsToRender() const
{
    return true;
}
