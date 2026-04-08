#include "CompositPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"
#include "Core/Global/Profiling.h"

using namespace RenderPasses;

CompositPass::CompositPass() : GenericGeometryPass("CompositPass")
{
    SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates::Complete);
    CreateSharedDescriptorLayout();
}

void CompositPass::Init(RendererAttachmentInfo& attachmentInfo,
                                      const SharedResourceManager& resourceManager)
{
    ScopedZone("CompositPass::Init");

    const auto swapChainAttachment =
        CreateDefaultColorAttachment(SWAPCHAIN_FORMAT, LoadOp::CLEAR, nullptr);
    m_mainRenderingData.colorAttachments = {swapChainAttachment};

    InitBaseData(attachmentInfo);
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
        m_indirectCmdBuffers[i].Init(10);
        
    BuildPipelines();
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

void CompositPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    const auto currentFrame = ctx.currentFrame;
    UpdateContextForFrame(currentFrame);

    ColorAttachment swapchainAttachment = m_mainRenderingData.colorAttachments[0];
    swapchainAttachment.SetTexture(ctx.pCurrentSwapchainTexture);

    stltype::vector<ColorAttachment> colorAttachments = {swapchainAttachment};

    const auto ex = ctx.pCurrentSwapchainTexture->GetInfo().extents;
    const DirectX::XMINT2 extents(ex.x, ex.y);

    BeginRenderingCmd cmdBegin{&m_mainPSO, ToRenderAttachmentInfos(colorAttachments)};
    cmdBegin.extents = extents;
    cmdBegin.viewport = data.mainView.viewport;

    auto& sceneGeometryBuffers = data.pResourceManager->GetSceneGeometryBuffers();
    if (sceneGeometryBuffers.GetVertexBuffer().GetRef() == VK_NULL_HANDLE ||
        sceneGeometryBuffers.GetIndexBuffer().GetRef() == VK_NULL_HANDLE)
    {
        return;
    }
    BinRenderDataCmd geomBufferCmd(sceneGeometryBuffers.GetVertexBuffer(), sceneGeometryBuffers.GetIndexBuffer());
    
    auto& cmdBuf = m_indirectCmdBuffers[m_currentFrameIdx];
    GenericIndirectDrawCmd cmd{&m_mainPSO, cmdBuf};
    cmd.drawCount = cmdBuf.GetDrawCmdNum();

    if (data.bufferDescriptors.empty() == false)
    {
        // For now, use same descriptors as lighting, will adjust as needed
        const auto transformSSBOSet = data.bufferDescriptors.at(UBO::DescriptorContentsType::GlobalInstanceData);
        const auto texArraySet = data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessTextureArray);
        const auto gbufferUBOSet = data.bufferDescriptors.at(UBO::DescriptorContentsType::GBuffer);
        cmd.descriptorSets = {texArraySet,
                               data.mainView.descriptorSet,
                               transformSSBOSet,
                               gbufferUBOSet};
    }

    StartRenderPassProfilingScope(pCmdBuffer);
    pCmdBuffer->RecordCommand(cmdBegin);
    pCmdBuffer->RecordCommand(geomBufferCmd);
    pCmdBuffer->RecordCommand(cmd);
    pCmdBuffer->RecordCommand(EndRenderingCmd{});
    EndRenderPassProfilingScope(pCmdBuffer);
}

void CompositPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalArrayTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, 1));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TransformSSBO, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GlobalObjectDataSSBOs, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::InstanceDataSSBO, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PrevTransformSSBO, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GBufferUBO, 3));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ShadowmapUBO, 3));
}

bool CompositPass::WantsToRender() const
{
    return true;
}
