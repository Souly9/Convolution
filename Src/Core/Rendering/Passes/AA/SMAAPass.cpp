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
#include <DirectXMath.h>

struct PushConsts {
    mathstl::Vector4 metrics; // { 1/w, 1/h, w, h }
    u32 tex1;
    u32 tex2;
    u32 tex3;
};

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
    edgeInfo.pushConstantInfo.constants = {{ShaderTypeBits::Vertex | ShaderTypeBits::Fragment, 0, (u32)sizeof(PushConsts)}};
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
    blendInfo.pushConstantInfo.constants = {{ShaderTypeBits::Vertex | ShaderTypeBits::Fragment, 0, (u32)sizeof(PushConsts)}};
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
    neighborInfo.pushConstantInfo.constants = {{ShaderTypeBits::Vertex | ShaderTypeBits::Fragment, 0, (u32)sizeof(PushConsts)}};
    neighborInfo.hasDepth = false;
    m_neighborhoodPSO = PSO(ShaderCollection{&neighborVert, &neighborFrag}, PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions}, neighborInfo);
}

bool SMAAPass::WantsToRender() const
{
    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    return renderState.aaType == AntialiasingType::TAA_SMAA;
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

void SMAAPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    ScopedZone("SMAAPass::Render");
    StartRenderPassProfilingScope(pCmdBuffer);

    UpdateContextForFrame(ctx.imageIdx);
    auto& cmdBuf = m_indirectCmdBuffers[m_currentFrameIdx];

    const auto extentsXY = data.renderState.renderResolution;
    const DirectX::XMINT2 extents(extentsXY.x, extentsXY.y);
    auto& sceneGeometryBuffers = data.pResourceManager->GetSceneGeometryBuffers();
    if (sceneGeometryBuffers.GetVertexBuffer().GetRef() == VK_NULL_HANDLE ||
        sceneGeometryBuffers.GetIndexBuffer().GetRef() == VK_NULL_HANDLE)
    {
        return;
    }
    
    BinRenderDataCmd geomBufferCmd(sceneGeometryBuffers.GetVertexBuffer(), sceneGeometryBuffers.GetIndexBuffer());
    
    auto gbufferUBOSet = data.bufferDescriptors.at(UBO::DescriptorContentsType::GBuffer);
    auto texArraySet = data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessTextureArray);

    PushConsts pc;
    pc.metrics = mathstl::Vector4(1.0f / extents.x, 1.0f / extents.y, (f32)extents.x, (f32)extents.y);

    // 1. Edge Detection (Writes to data.pSMAAEdgesTexture)
    {
        ColorAttachment attach = CreateDefaultColorAttachment(data.pSMAAEdgesTexture->GetInfo().format, LoadOp::CLEAR, data.pSMAAEdgesTexture);
        BeginRenderingCmd beginEdge{&m_edgePSO, ToRenderAttachmentInfos(stltype::vector<ColorAttachment>{attach})};
        beginEdge.extents = extents;
        beginEdge.viewport = data.mainView.viewport;
        
        GenericIndirectDrawCmd cmdEdge{&m_edgePSO, cmdBuf};
        cmdEdge.drawCount = cmdBuf.GetDrawCmdNum();
        cmdEdge.descriptorSets = { texArraySet, gbufferUBOSet };
        pc.tex1 = data.pGbuffer->GetHandle(GBufferTextureType::GBufferResolve); // Input color (TAA result)
        cmdEdge.SetPushConstants(0, pc, ShaderTypeBits::Vertex | ShaderTypeBits::Fragment);

        pCmdBuffer->RecordCommand(beginEdge);
        if (geomBufferCmd.vertexBuffer != nullptr)
            pCmdBuffer->RecordCommand(geomBufferCmd);
        pCmdBuffer->RecordCommand(cmdEdge);
        pCmdBuffer->RecordCommand(EndRenderingCmd{});
    }

    // Barrier between Edge and Blend: EdgesTex COLOR_ATTACHMENT -> SHADER_READ
    {
        ImageLayoutTransitionCmd barrier(data.pSMAAEdgesTexture);
        barrier.oldLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
        VkTextureManager::SetLayoutBarrierMasks(barrier, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        pCmdBuffer->RecordCommand(barrier);
    }

    // 2. Blend Weight Calculation (Writes to data.pSMAABlendTexture)
    {
        ColorAttachment attach = CreateDefaultColorAttachment(data.pSMAABlendTexture->GetInfo().format, LoadOp::CLEAR, data.pSMAABlendTexture);
        BeginRenderingCmd beginBlend{&m_blendPSO, ToRenderAttachmentInfos(stltype::vector<ColorAttachment>{attach})};
        beginBlend.extents = extents;
        beginBlend.viewport = data.mainView.viewport;

        GenericIndirectDrawCmd cmdBlend{&m_blendPSO, cmdBuf};
        cmdBlend.drawCount = cmdBuf.GetDrawCmdNum();
        cmdBlend.descriptorSets = { texArraySet, gbufferUBOSet };
        pc.tex1 = data.smaaEdges; // Edges
        pc.tex2 = m_areaTexBindless;
        pc.tex3 = m_searchTexBindless;
        cmdBlend.SetPushConstants(0, pc, ShaderTypeBits::Vertex | ShaderTypeBits::Fragment);

        pCmdBuffer->RecordCommand(beginBlend);
        if (geomBufferCmd.vertexBuffer != nullptr)
            pCmdBuffer->RecordCommand(geomBufferCmd);
        pCmdBuffer->RecordCommand(cmdBlend);
        pCmdBuffer->RecordCommand(EndRenderingCmd{});
    }

    // Barrier between Blend and Neighborhood: BlendTex COLOR_ATTACHMENT -> SHADER_READ
    {
        ImageLayoutTransitionCmd barrier(data.pSMAABlendTexture);
        barrier.oldLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
        VkTextureManager::SetLayoutBarrierMasks(barrier, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        pCmdBuffer->RecordCommand(barrier);
    }
    {
        // Output to GBufferThisFrameColor
        Texture* pOutputTexture = data.pGbuffer->Get(GBufferTextureType::GBufferThisFrameColor);
        
        ImageLayoutTransitionCmd outputBarrier(pOutputTexture);
        outputBarrier.oldLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
        outputBarrier.newLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
        VkTextureManager::SetLayoutBarrierMasks(outputBarrier, ImageLayout::SHADER_READ_ONLY_OPTIMAL, ImageLayout::COLOR_ATTACHMENT_OPTIMAL);
        pCmdBuffer->RecordCommand(outputBarrier);

        ColorAttachment attach = CreateDefaultColorAttachment(pOutputTexture->GetInfo().format, LoadOp::LOAD, pOutputTexture);
        BeginRenderingCmd beginNeighbor{&m_neighborhoodPSO, ToRenderAttachmentInfos(stltype::vector<ColorAttachment>{attach})};
        beginNeighbor.extents = extents;
        beginNeighbor.viewport = data.mainView.viewport;

        GenericIndirectDrawCmd cmdNeighbor{&m_neighborhoodPSO, cmdBuf};
        cmdNeighbor.drawCount = cmdBuf.GetDrawCmdNum();
        cmdNeighbor.descriptorSets = { texArraySet, gbufferUBOSet };
        pc.tex1 = data.pGbuffer->GetHandle(GBufferTextureType::GBufferResolve); // Un-AA color (TAA result)
        pc.tex2 = data.smaaBlend; // Blending weights
        cmdNeighbor.SetPushConstants(0, pc, ShaderTypeBits::Vertex | ShaderTypeBits::Fragment);

        pCmdBuffer->RecordCommand(beginNeighbor);
        if (geomBufferCmd.vertexBuffer != nullptr)
            pCmdBuffer->RecordCommand(geomBufferCmd);
        pCmdBuffer->RecordCommand(cmdNeighbor);
        pCmdBuffer->RecordCommand(EndRenderingCmd{});
        
        // Transition back to SHADER_READ for Composite
        ImageLayoutTransitionCmd outputBarrier2(pOutputTexture);
        outputBarrier2.oldLayout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
        outputBarrier2.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
        VkTextureManager::SetLayoutBarrierMasks(outputBarrier2, ImageLayout::COLOR_ATTACHMENT_OPTIMAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        pCmdBuffer->RecordCommand(outputBarrier2);
    }
    

    EndRenderPassProfilingScope(pCmdBuffer);
}
} // namespace RenderPasses
