#include "ClusterDebugPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Vulkan/Utils/VkDescriptorLayoutUtils.h"
#include "Core/Rendering/Core/Pipeline.h"
#include "Core/Rendering/Passes/Utils/RenderPassUtils.h"
#include "Core/Rendering/Vulkan/VkShader.h"
#include "Core/Rendering/Passes/PassManager.h" // For MainPassData definition

using namespace RenderPasses;
ClusterDebugPass::ClusterDebugPass() : ConvolutionRenderPass("ClusterDebugPass")
{
    m_indirectCmdBuffers.resize(SWAPCHAIN_IMAGES);
    CreateSharedDescriptorLayout();
}

void ClusterDebugPass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
{
    ScopedZone("ClusterDebugPass::Init");

    RecreateResolutionDependentResources(attachmentInfo, resourceManager);

    BuildBuffers();
    BuildPipelines();
}

void ClusterDebugPass::RecreateResolutionDependentResources(RendererAttachmentInfo& attachmentInfo,
                                                            const SharedResourceManager& resourceManager)
{
    ScopedZone("ClusterDebugPass::RecreateResolutionDependentResources");
    
    const auto& gbufferInfo = attachmentInfo.gbuffer;
    const auto gbufferDebug =
        CreateDefaultColorAttachment(gbufferInfo.GetFormat(GBufferTextureType::GBufferDebug), LoadOp::LOAD, nullptr);

    m_mainRenderingData.colorAttachments.clear();
    m_mainRenderingData.colorAttachments.push_back(std::move(gbufferDebug));
    m_mainRenderingData.depthAttachment =
        CreateReadOnlyDepthAttachment(LoadOp::LOAD, attachmentInfo.depthAttachment.GetTexture());
    
    InitBaseData(attachmentInfo);
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
    
    // Create indirect command buffers
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
        m_indirectCmdBuffers[i].Init(1);
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
    info.rasterizerInfo.cullmode = CullMode::NONE;
    
    // Depth test
    info.hasDepth = true;
    info.depthWriteEnable = false;
    
    m_pipeline = PSO(ShaderCollection{&vert, &frag}, PipeVertInfo{m_vertexInputDescription, {}}, info);
}

void ClusterDebugPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalArrayTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, 1));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ClusterAABBsSSBO, 2));
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

    m_currentFrameIdx = ctx.imageIdx;
    auto& cmdBuf = m_indirectCmdBuffers[m_currentFrameIdx];
    // Update Indirect Command
    cmdBuf.EmptyCmds();
    // indexCount = 24, instanceCount = totalClusters, firstIndex = 0, vertexOffset = 0, firstInstance = 0
    cmdBuf.AddIndexedDrawCmd(24, totalClusters, 0, 0, 0); 
    cmdBuf.FillCmds();
    
    // Begin Rendering
    const DirectX::XMINT2 extents(data.renderState.renderResolution.x, data.renderState.renderResolution.y);
    
    ColorAttachment gbufferDebug = m_mainRenderingData.colorAttachments[0];
    gbufferDebug.SetTexture(data.pGbuffer->Get(GBufferTextureType::GBufferDebug));
    
    stltype::vector<ColorAttachment> colorAttachments = {gbufferDebug};
    
    m_mainRenderingData.depthAttachment.SetTexture(data.pMainDepthTexture);
    BeginRenderingCmd cmdBegin{&m_pipeline, ToRenderAttachmentInfos(colorAttachments), ToRenderAttachmentInfo(m_mainRenderingData.depthAttachment)};
    cmdBegin.extents = extents;
    cmdBegin.viewport = data.mainView.viewport;
    
    GenericIndirectDrawCmd cmd{&m_pipeline, cmdBuf};
    cmd.drawCount = 1;
    
    if (data.bufferDescriptors.empty())
        cmd.descriptorSets = {DescriptorSet::Cast(g_pTexManager->GetBindlessDescriptorSet()), data.mainView.descriptorSet, ctx.clusterGridDescriptor};
    else
    {
        const auto texArraySet = data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessTextureArray);
        const auto clusterGridSet = data.bufferDescriptors.at(UBO::DescriptorContentsType::ClusterGrid);
        cmd.descriptorSets = {texArraySet, data.mainView.descriptorSet, clusterGridSet};
    }
    
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

