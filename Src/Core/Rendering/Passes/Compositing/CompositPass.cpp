#include "CompositPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/View.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Global/Profiling.h"
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

    const auto swapChainAttachment =
        CreateDefaultColorAttachment(SWAPCHAIN_FORMAT, LoadOp::CLEAR, nullptr);
    m_mainRenderingData.colorAttachments = {swapChainAttachment};

    InitBaseData();
}

void CompositPass::BuildPipelines()
{
    ScopedZone("CompositPass::BuildPipelines");
    auto mainVert = Shader("Shaders/CompositPass.vert.spv", "main");
    auto mainFrag = Shader("Shaders/CompositPass.frag.spv", "main");

    PipelineInfo info{};
    info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
    info.attachmentInfos = CreateAttachmentInfo({m_mainRenderingData.colorAttachments});
    info.hasDepth = false;
    m_mainPSO = PSO(
        ShaderCollection{&mainVert, &mainFrag}, PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions}, info);
}

void CompositPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
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

void CompositPass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("CompositPass::Render");

    CommandBuffer* pCmdBuffer = execCtx.pCmdBuffer;
    ColorAttachment swapchainAttachment = m_mainRenderingData.colorAttachments[0];

    const auto& appRenderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    const bool smaaActive = (appRenderState.aaType == AntialiasingType::SMAA || appRenderState.aaType == AntialiasingType::TAA_SMAA);
    if (smaaActive)
    {
        Texture* pTarget = execCtx.GetTexture(RGResourceID::GBufferPostAAColor);
        swapchainAttachment.SetTexture(pTarget);
    }
    else
    {
        Texture* pTarget = execCtx.GetTexture(RGResourceID::Swapchain);
        swapchainAttachment.SetTexture(pTarget != nullptr ? pTarget : ctx.pCurrentSwapchainTexture);
    }

    stltype::vector<ColorAttachment> colorAttachments = {swapchainAttachment};

    const auto ex = swapchainAttachment.GetTexture() ? swapchainAttachment.GetTexture()->GetInfo().extents : ctx.pCurrentSwapchainTexture->GetInfo().extents;
    const DirectX::XMINT2 extents(ex.x, ex.y);

    BeginRenderingCmd cmdBegin{&m_mainPSO, ToRenderAttachmentInfos(colorAttachments)};
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
    GenericIndirectDrawCmd cmd{&m_mainPSO, cmdBuf};
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

    builder.ReadTexture(RGResourceID::TemporalResolve, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::GBufferThisFrameColor, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::BloomMip0, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    const auto& appRenderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    const bool smaaActive = (appRenderState.aaType == AntialiasingType::SMAA || appRenderState.aaType == AntialiasingType::TAA_SMAA);
    if (smaaActive)
    {
        builder.WriteColorAttachment(RGResourceID::GBufferPostAAColor, LoadOp::CLEAR, StoreOp::STORE);
    }
    else
    {
        builder.WriteColorAttachment(RGResourceID::Swapchain, LoadOp::CLEAR, StoreOp::STORE);
    }
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
