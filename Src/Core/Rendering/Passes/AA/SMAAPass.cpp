#include "SMAAPass.h"
#include "../../../../../Shaders/Globals/PushConstants.h"
#include "AreaTex.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"
#include "Core/Rendering/Core/AntiAliasing.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "SearchTex.h"
#include <DirectXMath.h>

namespace RenderPasses
{

SMAAPass::SMAAPass() : GenericGeometryPass("SMAAPass")
{
    SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates::Complete);
    CreateSharedDescriptorLayout();
}

SMAAPass::~SMAAPass()
{
}

void SMAAPass::Init(const SharedResourceManager& resourceManager)
{
    ScopedZone("SMAAPass::Init");
    InitBaseData();

    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        m_indirectCmdBuffers[i].Init(10);
    }

    // Upload SMAA textures

    auto searchHandle = g_renderer.GetTextureManager().SubmitAsyncTextureCreation(
        {"Resources\\Textures\\SearchTex.dds", false, TextureSemantic::Data, true});

    ReadTextureInfo areaTexInfo{};
    areaTexInfo.pixels = (unsigned char*)areaTexBytes;
    areaTexInfo.extents = {AREATEX_WIDTH, AREATEX_HEIGHT};
    areaTexInfo.dataSize = AREATEX_SIZE;
    areaTexInfo.autoFree = false;
    areaTexInfo.filePath = "SMAA_AreaTex";

    FileTextureRequest areaReq{};
    areaReq.ioInfo = areaTexInfo;
    areaReq.handle = g_renderer.GetTextureManager().GenerateHandle();
    areaReq.makeBindless = true;
    areaReq.isPersistent = true;
    areaReq.format = TexFormat::R8G8_UNORM;
    areaReq.semantic = TextureSemantic::Data;

    g_renderer.GetTextureManager().CreateTexture(areaReq);

    m_searchTexBindless = g_renderer.GetTextureManager().MakeTextureBindless(searchHandle, true);
    m_areaTexBindless = g_renderer.GetTextureManager().MakeTextureBindless(areaReq.handle, true);

    BuildPipelines();
}

void SMAAPass::BuildPipelines()
{
    ScopedZone("SMAAPass::BuildPipelines");

    // 1. Edge Detection PSO
    auto edgeVert = Shader("Shaders/SMAAEdge.vert.spv", "main");
    auto edgeFrag = Shader("Shaders/SMAAEdge.frag.spv", "main");
    PipelineInfo edgeInfo{};
    edgeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
    edgeInfo.attachmentInfos.colorAttachments = { TexFormat::R8G8_UNORM };
    edgeInfo.pushConstantInfo.constants = {
        {ShaderTypeBits::Vertex | ShaderTypeBits::Fragment, 0, (u32)sizeof(SMAAPushConstants)}};
    edgeInfo.hasDepth = false;
    m_edgePSO = PSO(ShaderCollection{&edgeVert, &edgeFrag},
                    PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions},
                    edgeInfo);

    // 2. Blend Weight Calculation PSO
    auto blendVert = Shader("Shaders/SMAABlend.vert.spv", "main");
    auto blendFrag = Shader("Shaders/SMAABlend.frag.spv", "main");
    PipelineInfo blendInfo{};
    blendInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
    blendInfo.attachmentInfos.colorAttachments = { TexFormat::R8G8B8A8_UNORM };
    blendInfo.pushConstantInfo.constants = {
        {ShaderTypeBits::Vertex | ShaderTypeBits::Fragment, 0, (u32)sizeof(SMAAPushConstants)}};
    blendInfo.hasDepth = false;
    m_blendPSO = PSO(ShaderCollection{&blendVert, &blendFrag},
                     PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions},
                     blendInfo);

    // 3. Neighborhood Blending PSO
    auto neighborVert = Shader("Shaders/SMAANeighborhood.vert.spv", "main");
    auto neighborFrag = Shader("Shaders/SMAANeighborhood.frag.spv", "main");
    PipelineInfo neighborInfo{};
    neighborInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
    neighborInfo.attachmentInfos.colorAttachments = { g_renderer.GetSwapchainFormat() };
    neighborInfo.pushConstantInfo.constants = {
        {ShaderTypeBits::Vertex | ShaderTypeBits::Fragment, 0, (u32)sizeof(SMAAPushConstants)}};
    neighborInfo.hasDepth = false;
    m_neighborhoodPSO = PSO(ShaderCollection{&neighborVert, &neighborFrag},
                            PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions},
                            neighborInfo);
}

bool SMAAPass::WantsToRender() const
{
    return AA::Current().smaa;
}

void SMAAPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset(DescriptorPresets::Bindless(false));
    AppendLayoutPreset(DescriptorPresets::GBuffer());
}

void SMAAPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                   FrameRendererContext& previousFrameCtx,
                                   u32 thisFrameNum)
{
    m_currentFrameIdx = thisFrameNum % SWAPCHAIN_IMAGES;
    auto& cmdBuf = m_indirectCmdBuffers.at(m_currentFrameIdx);
    cmdBuf.EmptyCmds();
    const auto pFullScreenQuadMesh = g_engine.GetMeshManager().GetPrimitiveMesh(MeshManager::PrimitiveType::Quad);
    const auto meshHandle = previousFrameCtx.pResourceManager->GetMeshHandle(pFullScreenQuadMesh);
    cmdBuf.AddIndexedDrawCmd(meshHandle.indexCount, 1, meshHandle.indexBufferOffset, meshHandle.vertBufferOffset, 0);
    RebuildPerObjectBuffer({0});
    cmdBuf.FillCmds();
}

void SMAAPass::RenderWithGraph(const MainPassData& data,
                               const FrameRendererContext& ctx,
                               const RGExecutionContext& execCtx)
{
    ScopedZone("SMAAPass::Render");
    StartRenderPassProfilingScope(execCtx.pCmdBuffer);

    UpdateContextForFrame(ctx.currentFrame);
    auto& cmdBuf = m_indirectCmdBuffers[m_currentFrameIdx];

    const auto extentsXY = data.renderState.swapchainResolution;
    const DirectX::XMINT2 extents(extentsXY.x, extentsXY.y);
    const auto displayViewport = RenderViewUtils::CreateViewportFromData(extentsXY, ctx.zNear, ctx.zFar);
    auto& sceneGeometryBuffers = data.pResourceManager->GetSceneGeometryBuffers();
    if (!sceneGeometryBuffers.GetVertexBuffer().IsCreated() || !sceneGeometryBuffers.GetIndexBuffer().IsCreated())
    {
        return;
    }

    BinRenderDataCmd geomBufferCmd(sceneGeometryBuffers.GetVertexBuffer(), sceneGeometryBuffers.GetIndexBuffer());

    SMAAPushConstants pc;
    pc.metrics = mathstl::Vector4(1.0f / extents.x, 1.0f / extents.y, (f32)extents.x, (f32)extents.y);

    const u32 inputColorHandle = execCtx.GetBindless(RGResourceID::GBufferPostAAColor);

    // 1. Edge Detection
    {
        RenderAttachmentInfo edgeAttachment = execCtx.GetColorAttachment(RGResourceID::SMAAEdges, LoadOp::CLEAR, StoreOp::STORE);
        BeginRenderingCmd beginEdges{&m_edgePSO, {edgeAttachment}};
        beginEdges.extents = extents;
        beginEdges.viewport = displayViewport;

        GenericIndirectDrawCmd cmdEdges{&m_edgePSO, cmdBuf};
        cmdEdges.drawCount = cmdBuf.GetDrawCmdNum();
        cmdEdges.descriptorSets = execCtx.GetDescriptors();
        pc.tex1 = inputColorHandle;
        pc.tex2 = m_searchTexBindless;
        cmdEdges.SetPushConstants(0, pc, ShaderTypeBits::Vertex | ShaderTypeBits::Fragment);

        execCtx.pCmdBuffer->RecordCommand(beginEdges);
        if (geomBufferCmd.vertexBuffer != nullptr)
            execCtx.pCmdBuffer->RecordCommand(geomBufferCmd);
        execCtx.pCmdBuffer->RecordCommand(cmdEdges);
        execCtx.pCmdBuffer->RecordCommand(EndRenderingCmd{});
    }

    // 2. Blending Weight Calculation
    {
        RenderAttachmentInfo blendAttachment = execCtx.GetColorAttachment(RGResourceID::SMAABlend, LoadOp::CLEAR, StoreOp::STORE);
        BeginRenderingCmd beginBlend{&m_blendPSO, {blendAttachment}};
        beginBlend.extents = extents;
        beginBlend.viewport = displayViewport;

        GenericIndirectDrawCmd cmdBlend{&m_blendPSO, cmdBuf};
        cmdBlend.drawCount = cmdBuf.GetDrawCmdNum();
        cmdBlend.descriptorSets = execCtx.GetDescriptors();
        pc.tex1 = execCtx.GetBindless(RGResourceID::SMAAEdges);
        pc.tex2 = m_areaTexBindless;
        pc.tex3 = m_searchTexBindless;
        cmdBlend.SetPushConstants(0, pc, ShaderTypeBits::Vertex | ShaderTypeBits::Fragment);

        execCtx.pCmdBuffer->RecordCommand(beginBlend);
        if (geomBufferCmd.vertexBuffer != nullptr)
            execCtx.pCmdBuffer->RecordCommand(geomBufferCmd);
        execCtx.pCmdBuffer->RecordCommand(cmdBlend);
        execCtx.pCmdBuffer->RecordCommand(EndRenderingCmd{});
    }

    // 3. Neighborhood Blending
    {
        RenderAttachmentInfo neighborAttachment = execCtx.GetColorAttachment(RGResourceID::Swapchain, LoadOp::LOAD, StoreOp::STORE);
        if (!neighborAttachment.pTexture)
            neighborAttachment.pTexture = ctx.pCurrentSwapchainTexture;
        BeginRenderingCmd beginNeighbor{
            &m_neighborhoodPSO, {neighborAttachment}};
        beginNeighbor.extents = extents;
        beginNeighbor.viewport = displayViewport;

        GenericIndirectDrawCmd cmdNeighbor{&m_neighborhoodPSO, cmdBuf};
        cmdNeighbor.drawCount = cmdBuf.GetDrawCmdNum();
        cmdNeighbor.descriptorSets = execCtx.GetDescriptors();
        pc.tex1 = inputColorHandle; // Post-tonemap LDR color
        pc.tex2 = execCtx.GetBindless(RGResourceID::SMAABlend);
        cmdNeighbor.SetPushConstants(0, pc, ShaderTypeBits::Vertex | ShaderTypeBits::Fragment);

        execCtx.pCmdBuffer->RecordCommand(beginNeighbor);
        if (geomBufferCmd.vertexBuffer != nullptr)
            execCtx.pCmdBuffer->RecordCommand(geomBufferCmd);
        execCtx.pCmdBuffer->RecordCommand(cmdNeighbor);
        execCtx.pCmdBuffer->RecordCommand(EndRenderingCmd{});

        m_outputWritten = true;
    }

    EndRenderPassProfilingScope(execCtx.pCmdBuffer);
}

#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"

void SMAAPass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.DeclareContexts<
        PassCtx::Bindless,
        PassCtx::GBufferCtx>();

    builder.ReadTexture(RGResourceID::GBufferPostAAColor,
                        SyncStages::FRAGMENT_SHADER,
                        AccessFlags::SHADER_READ,
                        ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    auto smaaEdges =
        builder.DeclareStorageTexture(RGResourceID::SMAAEdges, TexFormat::R8G8_UNORM, RGSizeClass::OutputResolution);
    auto smaaBlend = builder.DeclareStorageTexture(
        RGResourceID::SMAABlend, TexFormat::R8G8B8A8_UNORM, RGSizeClass::OutputResolution);

    builder.WriteColorAttachment(smaaEdges, LoadOp::CLEAR, StoreOp::STORE);
    builder.WriteColorAttachment(smaaBlend, LoadOp::CLEAR, StoreOp::STORE);
    builder.WriteColorAttachment(RGResourceID::Swapchain, LoadOp::LOAD, StoreOp::STORE);
    builder.SetHasSideEffects();
}
} // namespace RenderPasses
