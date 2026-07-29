#include "SMAAPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Core/Global/Profiling.h"
#include "AreaTex.h"
#include "SearchTex.h"
#include "../../../../../Shaders/Globals/PushConstants.h"
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

void SMAAPass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
{
    ScopedZone("SMAAPass::Init");
    InitBaseData(attachmentInfo);
    
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        m_indirectCmdBuffers[i].Init(10);
    }
        
    // Upload SMAA textures

    auto searchHandle = g_pTexManager->SubmitAsyncTextureCreation(
        {"Resources\\Textures\\SearchTex.dds", false, TextureSemantic::Data, true});
    
    ReadTextureInfo areaTexInfo{};
    areaTexInfo.pixels = (unsigned char*)areaTexBytes;
    areaTexInfo.extents = {AREATEX_WIDTH, AREATEX_HEIGHT};
    areaTexInfo.dataSize = AREATEX_SIZE;
    areaTexInfo.autoFree = false;
    areaTexInfo.filePath = "SMAA_AreaTex";

    FileTextureRequest areaReq{};
    areaReq.ioInfo = areaTexInfo;
    areaReq.handle = g_pTexManager->GenerateHandle();
    areaReq.makeBindless = true;
    areaReq.isPersistent = true;
    areaReq.format = TexFormat::R8G8_UNORM;
    areaReq.semantic = TextureSemantic::Data;

    g_pTexManager->SubmitTextureRequest(areaReq);
    
    m_searchTexBindless = g_pTexManager->MakeTextureBindless(searchHandle, true);
    m_areaTexBindless = g_pTexManager->MakeTextureBindless(areaReq.handle, true);

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
    edgeInfo.attachmentInfos = CreateAttachmentInfo({
        CreateDefaultColorAttachment(TexFormat::R8G8_UNORM, LoadOp::CLEAR, nullptr)
    });
    edgeInfo.pushConstantInfo.constants = {{ShaderTypeBits::Vertex | ShaderTypeBits::Fragment, 0, (u32)sizeof(SMAAPushConstants)}};
    edgeInfo.hasDepth = false;
    m_edgePSO = PSO(ShaderCollection{&edgeVert, &edgeFrag}, PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions}, edgeInfo);

    // 2. Blend Weight Calculation PSO
    auto blendVert = Shader("Shaders/SMAABlend.vert.spv", "main");
    auto blendFrag = Shader("Shaders/SMAABlend.frag.spv", "main");
    PipelineInfo blendInfo{};
    blendInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
    blendInfo.attachmentInfos = CreateAttachmentInfo({
        CreateDefaultColorAttachment(TexFormat::R8G8B8A8_UNORM, LoadOp::CLEAR, nullptr)
    });
    blendInfo.pushConstantInfo.constants = {{ShaderTypeBits::Vertex | ShaderTypeBits::Fragment, 0, (u32)sizeof(SMAAPushConstants)}};
    blendInfo.hasDepth = false;
    m_blendPSO = PSO(ShaderCollection{&blendVert, &blendFrag}, PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions}, blendInfo);

    // 3. Neighborhood Blending PSO
    auto neighborVert = Shader("Shaders/SMAANeighborhood.vert.spv", "main");
    auto neighborFrag = Shader("Shaders/SMAANeighborhood.frag.spv", "main");
    PipelineInfo neighborInfo{};
    neighborInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
    neighborInfo.attachmentInfos = CreateAttachmentInfo({
        CreateDefaultColorAttachment(TexFormat::R16G16B16A16_FLOAT, LoadOp::LOAD, nullptr)
    });
    neighborInfo.pushConstantInfo.constants = {{ShaderTypeBits::Vertex | ShaderTypeBits::Fragment, 0, (u32)sizeof(SMAAPushConstants)}};
    neighborInfo.hasDepth = false;
    m_neighborhoodPSO = PSO(ShaderCollection{&neighborVert, &neighborFrag}, PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions}, neighborInfo);
}

bool SMAAPass::WantsToRender() const
{
    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    return renderState.aaType == AntialiasingType::SMAA || renderState.aaType == AntialiasingType::TAA_SMAA;
}

void SMAAPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalArrayTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GBufferUBO, 1));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ShadowmapUBO, 1));
}

void SMAAPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                   FrameRendererContext& previousFrameCtx,
                                   u32 thisFrameNum)
{
    m_currentFrameIdx = thisFrameNum % SWAPCHAIN_IMAGES;
    auto& cmdBuf = m_indirectCmdBuffers.at(m_currentFrameIdx);
    cmdBuf.EmptyCmds();
    const auto pFullScreenQuadMesh = g_pMeshManager->GetPrimitiveMesh(MeshManager::PrimitiveType::Quad);
    const auto meshHandle = previousFrameCtx.pResourceManager->GetMeshHandle(pFullScreenQuadMesh);
    cmdBuf.AddIndexedDrawCmd(meshHandle.indexCount, 1, meshHandle.indexBufferOffset, meshHandle.vertBufferOffset, 0);
    RebuildPerObjectBuffer({0});
    cmdBuf.FillCmds();
}

void SMAAPass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("SMAAPass::Render");
    StartRenderPassProfilingScope(execCtx.pCmdBuffer);

    UpdateContextForFrame(ctx.currentFrame);
    auto& cmdBuf = m_indirectCmdBuffers[m_currentFrameIdx];

    const auto extentsXY = data.renderState.swapchainResolution;
    const DirectX::XMINT2 extents(extentsXY.x, extentsXY.y);
    const auto displayViewport = RenderViewUtils::CreateViewportFromData(extentsXY, ctx.zNear, ctx.zFar);
    auto& sceneGeometryBuffers = data.pResourceManager->GetSceneGeometryBuffers();
    if (!sceneGeometryBuffers.GetVertexBuffer().IsCreated() ||
        !sceneGeometryBuffers.GetIndexBuffer().IsCreated())
    {
        return;
    }
    
    BinRenderDataCmd geomBufferCmd(sceneGeometryBuffers.GetVertexBuffer(), sceneGeometryBuffers.GetIndexBuffer());
    
    auto gbufferUBOSet = data.bufferDescriptors.at(UBO::DescriptorContentsType::GBuffer);
    auto texArraySet = data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessTextureArray);

    SMAAPushConstants pc;
    pc.metrics = mathstl::Vector4(1.0f / extents.x, 1.0f / extents.y, (f32)extents.x, (f32)extents.y);

    const u32 inputColorHandle = execCtx.GetBindless(RGResourceID::GBufferPostAAColor);

    static bool loggedOnce = false;
    if (!loggedOnce)
    {
        DEBUG_LOGF("[SMAAPass] Input color handle selected: %u", inputColorHandle);
        loggedOnce = true;
    }

    // 1. Edge Detection
    {
        Texture* pEdgesTex = execCtx.GetTexture(RGResourceID::SMAAEdges);
        ColorAttachment attach = CreateDefaultColorAttachment(pEdgesTex ? pEdgesTex->GetInfo().format : TexFormat::R8G8_UNORM, LoadOp::CLEAR, ImageLayout::SHADER_READ_ONLY_OPTIMAL, pEdgesTex);
        BeginRenderingCmd beginEdges{&m_edgePSO, ToRenderAttachmentInfos(stltype::vector<ColorAttachment>{attach})};
        beginEdges.extents = extents;
        beginEdges.viewport = displayViewport;

        GenericIndirectDrawCmd cmdEdges{&m_edgePSO, cmdBuf};
        cmdEdges.drawCount = cmdBuf.GetDrawCmdNum();
        cmdEdges.descriptorSets = { texArraySet, gbufferUBOSet };
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
        Texture* pBlendTex = execCtx.GetTexture(RGResourceID::SMAABlend);
        ColorAttachment attach = CreateDefaultColorAttachment(pBlendTex ? pBlendTex->GetInfo().format : TexFormat::R8G8B8A8_UNORM, LoadOp::CLEAR, ImageLayout::SHADER_READ_ONLY_OPTIMAL, pBlendTex);
        BeginRenderingCmd beginBlend{&m_blendPSO, ToRenderAttachmentInfos(stltype::vector<ColorAttachment>{attach})};
        beginBlend.extents = extents;
        beginBlend.viewport = displayViewport;

        GenericIndirectDrawCmd cmdBlend{&m_blendPSO, cmdBuf};
        cmdBlend.drawCount = cmdBuf.GetDrawCmdNum();
        cmdBlend.descriptorSets = { texArraySet, gbufferUBOSet };
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
        Texture* pOutputTexture = ctx.pCurrentSwapchainTexture;
        ColorAttachment attach = CreateDefaultColorAttachment(pOutputTexture ? pOutputTexture->GetInfo().format : SWAPCHAIN_FORMAT, LoadOp::CLEAR, pOutputTexture);
        BeginRenderingCmd beginNeighbor{&m_neighborhoodPSO, ToRenderAttachmentInfos(stltype::vector<ColorAttachment>{attach})};
        beginNeighbor.extents = extents;
        beginNeighbor.viewport = displayViewport;

        GenericIndirectDrawCmd cmdNeighbor{&m_neighborhoodPSO, cmdBuf};
        cmdNeighbor.drawCount = cmdBuf.GetDrawCmdNum();
        cmdNeighbor.descriptorSets = { texArraySet, gbufferUBOSet };
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
    builder.ReadTexture(RGResourceID::GBufferPostAAColor, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    auto smaaEdges = builder.DeclareStorageTexture(RGResourceID::SMAAEdges, TexFormat::R8G8_UNORM, RGSizeClass::OutputResolution);
    auto smaaBlend = builder.DeclareStorageTexture(RGResourceID::SMAABlend, TexFormat::R8G8B8A8_UNORM, RGSizeClass::OutputResolution);

    builder.WriteColorAttachment(smaaEdges, LoadOp::CLEAR, StoreOp::STORE);
    builder.WriteColorAttachment(smaaBlend, LoadOp::CLEAR, StoreOp::STORE);
    builder.WriteColorAttachment(RGResourceID::Swapchain, LoadOp::CLEAR, StoreOp::STORE);
    builder.SetHasSideEffects();
}
} // namespace RenderPasses
