#include "ClusterDebugPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/Utils/DescriptorLayoutUtils.h"
#include "Core/Rendering/Core/Pipeline.h"
#include "Core/Rendering/Passes/Utils/RenderPassUtils.h"
#include "Core/Rendering/Vulkan/VkShader.h"
#include "Core/Rendering/Passes/PassManager.h" // For MainPassData definition

using namespace RenderPasses;
ClusterDebugPass::ClusterDebugPass() : ConvolutionRenderPass("ClusterDebugPass")
{
    CreateSharedDescriptorLayout();
}

void ClusterDebugPass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
{
    ScopedZone("ClusterDebugPass::Init");
    
    const auto& gbufferInfo = attachmentInfo.gbuffer;
     const auto gbufferPositionTex = 
        CreateDefaultColorAttachment(gbufferInfo.GetFormat(GBufferTextureType::GBufferAlbedo), LoadOp::LOAD, nullptr);
    const auto gbufferNormal =
        CreateDefaultColorAttachment(gbufferInfo.GetFormat(GBufferTextureType::GBufferNormal), LoadOp::LOAD, nullptr);
    const auto gbuffer3 = CreateDefaultColorAttachment(
        gbufferInfo.GetFormat(GBufferTextureType::TexCoordMatData), LoadOp::LOAD, nullptr);
    const auto gbufferPos =
        CreateDefaultColorAttachment(gbufferInfo.GetFormat(GBufferTextureType::Position), LoadOp::LOAD, nullptr);
        
    m_mainRenderingData.colorAttachments = {gbufferPositionTex, gbufferNormal, gbuffer3, gbufferPos};
    m_mainRenderingData.depthAttachment =
        CreateDefaultDepthAttachment(LoadOp::LOAD, attachmentInfo.depthAttachment.GetTexture());
    
    InitBaseData(attachmentInfo);
    
    BuildBuffers();
    BuildPipelines();
}

void ClusterDebugPass::BuildBuffers()
{
    // Create index buffer for a cube (12 lines)
    // 0-1, 1-2, 2-3, 3-0 (Bottom)
    // 4-5, 5-6, 6-7, 7-4 (Top)
    // 0-4, 1-5, 2-6, 3-7 (Sides)
    stltype::vector<u32> indices = {
        0, 1, 1, 2, 2, 3, 3, 0,
        4, 5, 5, 6, 6, 7, 7, 4,
        0, 4, 1, 5, 2, 6, 3, 7
    };
    
    m_indexBuffer = IndexBufferVulkan(indices.size() * sizeof(u32));
    m_indexBuffer.FillImmediate(indices.data());
    
    // Create dummy vertex buffer
    m_dummyVertexBuffer = VertexBufferVulkan(4); // 4 bytes, just to be valid
    u32 dummyData = 0;
    m_dummyVertexBuffer.FillImmediate(&dummyData);
    
    // Create indirect command buffer
    // Start with 1 command capacity
    m_indirectCmdBuffer = IndirectDrawCommandBuffer(1);
}

void ClusterDebugPass::BuildPipelines()
{
    ScopedZone("ClusterDebugPass::BuildPipelines");
    
    auto vert = Shader("Shaders/ClusterDebug.vert.spv", "main");
    auto frag = Shader("Shaders/ClusterDebug.frag.spv", "main");
    
    PipelineInfo info{};
    info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
    
    // Match the attachments
    info.attachmentInfos =
        CreateAttachmentInfo({m_mainRenderingData.colorAttachments}, m_mainRenderingData.depthAttachment);
        
    // Topology: Line List
    info.topology = Topology::Lines;
    info.rasterizerInfo.polyMode = PolygonMode::Line;
    info.rasterizerInfo.cullmode = Cullmode::None;
    
    // Depth test
    info.hasDepth = true;
    
    m_pipeline = PSO(ShaderCollection{&vert, &frag}, PipeVertInfo{m_vertexInputDescription, {}}, info);
}

void ClusterDebugPass::CreateSharedDescriptorLayout()
{
    // Set 0: ViewUBO
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, 0));
    // Set 1: ClusterGrid
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ClusterAABBsSSBO, 1));
}

void ClusterDebugPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                           FrameRendererContext& previousFrameCtx,
                                           u32 thisFrameNum)
{
    // Nothing to rebuild from scene meshes
}

void ClusterDebugPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    ScopedZone("ClusterDebugPass::Render");
    
    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    if (!renderState.showClusterAABBs)
        return;
        
    u32 totalClusters = renderState.totalClusterCount;
    if (totalClusters == 0) return;

    // Update Indirect Command
    m_indirectCmdBuffer.EmptyCmds();
    // indexCount = 24, instanceCount = totalClusters, firstIndex = 0, vertexOffset = 0, firstInstance = 0
    m_indirectCmdBuffer.AddIndexedDrawCmd(24, totalClusters, 0, 0, 0); 
    m_indirectCmdBuffer.FillCmds();
    
    // Begin Rendering
    const auto ex = ctx.pCurrentSwapchainTexture->GetInfo().extents;
    const DirectX::XMINT2 extents(ex.x, ex.y);
    
    ColorAttachment gbufferPosition = m_mainRenderingData.colorAttachments[0];
    ColorAttachment gbufferNormal = m_mainRenderingData.colorAttachments[1];
    ColorAttachment gbuffer3 = m_mainRenderingData.colorAttachments[2];
    ColorAttachment gbufferPos = m_mainRenderingData.colorAttachments[3];
    gbufferPosition.SetTexture(data.pGbuffer->Get(GBufferTextureType::GBufferAlbedo));
    gbufferNormal.SetTexture(data.pGbuffer->Get(GBufferTextureType::GBufferNormal));
    gbuffer3.SetTexture(data.pGbuffer->Get(GBufferTextureType::TexCoordMatData));
    gbufferPos.SetTexture(data.pGbuffer->Get(GBufferTextureType::Position));
    
    stltype::vector<ColorAttachment> colorAttachments = {gbufferPosition, gbufferNormal, gbuffer3, gbufferPos};
    
    BeginRenderingCmd cmdBegin{&m_pipeline, colorAttachments, &m_mainRenderingData.depthAttachment};
    cmdBegin.extents = extents;
    cmdBegin.viewport = data.mainView.viewport;
    
    GenericIndirectDrawCmd cmd{&m_pipeline, m_indirectCmdBuffer};
    cmd.drawCount = 1;
    cmd.descriptorSets = {ctx.mainViewUBODescriptor, ctx.clusterGridDescriptor};
    
    StartRenderPassProfilingScope(pCmdBuffer);
    pCmdBuffer->RecordCommand(cmdBegin);
    
    // Bind specific buffers
    BinRenderDataCmd bindStateCmd(m_dummyVertexBuffer, m_indexBuffer);
    pCmdBuffer->RecordCommand(bindStateCmd);
    
    pCmdBuffer->RecordCommand(cmd);
    pCmdBuffer->RecordCommand(EndRenderingCmd{});
    EndRenderPassProfilingScope(pCmdBuffer);
}

bool ClusterDebugPass::WantsToRender() const
{
    return g_pApplicationState->GetCurrentApplicationState().renderState.showClusterAABBs;
}

