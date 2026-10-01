#include "ClusterDebugPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/GBuffer.h"
#include "Core/Rendering/Core/Pipeline.h"
#include "Core/Rendering/Passes/PassManager.h" // For MainPassData definition
#include "Core/Rendering/Passes/Utils/RenderPassUtils.h"
#include "Core/Rendering/Core/DescriptorUtils/DescriptorLayoutUtils.h"
#include "Core/Rendering/Core/Shader.h"

using namespace RenderPasses;
ClusterDebugPass::ClusterDebugPass() : ConvolutionRenderPass("ClusterDebugPass")
{
    m_indirectCmdBuffers.resize(SWAPCHAIN_IMAGES);
    CreateSharedDescriptorLayout();
}

void ClusterDebugPass::Init(const SharedResourceManager& resourceManager)
{
    ScopedZone("ClusterDebugPass::Init");

    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
        m_indirectCmdBuffers[i].Init(1000000);

    RecreateResolutionDependentResources(resourceManager);
    BuildPipelines();
}

void ClusterDebugPass::RecreateResolutionDependentResources(const SharedResourceManager& resourceManager)
{
    ScopedZone("ClusterDebugPass::RecreateResolutionDependentResources");
    InitBaseData();
}

void ClusterDebugPass::BuildBuffers()
{
    // Create index buffer for a cube (12 lines)
    // 0-1, 1-2, 2-3, 3-0 (Bottom)
    // 4-5, 5-6, 6-7, 7-4 (Top)
    // 0-4, 1-5, 2-6, 3-7 (Sides)
    stltype::vector<u32> indices = {0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7};

    // Filled through a mapping below
    m_indexBuffer = IndexBuffer(indices.size() * sizeof(u32), true);
    m_indexBuffer.FillImmediate(indices.data());

    // Create dummy vertex buffer
    m_dummyVertexBuffer = VertexBuffer(4, true); // 4 bytes, just to be valid
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

    info.attachmentInfos.colorAttachments = { TexFormat::R16G16B16A16_FLOAT };
    info.attachmentInfos.depthAttachmentFormat = TexFormat::D32_SFLOAT;

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

void ClusterDebugPass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.DeclareContexts<
        PassCtx::Bindless,
        PassCtx::View,
        PassCtx::ClusterGrid>();

    builder.WriteGBuffer(RGResourceID::GBufferDebug, LoadOp::LOAD);
    builder.ReadDepth(RGResourceID::MainDepth);
    auto clusterGrid = builder.DeclareStorageBuffer(RGResourceID::ClusterGridBuffer, UBO::ClusterAABBSetSize);
    builder.ReadStorageBuffer(clusterGrid, SyncStages::VERTEX_SHADER, AccessFlags::SHADER_READ);
    builder.SetHasSideEffects();
}

void ClusterDebugPass::RenderWithGraph(const MainPassData& data,
                                       const FrameRendererContext& ctx,
                                       const RGExecutionContext& execCtx)
{
    ScopedZone("ClusterDebugPass::Render");

    const auto& renderState = g_engine.GetApplicationState().GetCurrentApplicationState().renderState;
    if (!mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::ShowClusterAABBs))
        return;

    u32 totalClusters = renderState.totalClusterCount;
    if (totalClusters == 0)
        return;

    m_currentFrameIdx = execCtx.GetFrameIndex();
    auto& cmdBuf = m_indirectCmdBuffers[m_currentFrameIdx];
    cmdBuf.EmptyCmds();
    cmdBuf.AddIndexedDrawCmd(24, totalClusters, 0, 0, 0);
    cmdBuf.FillCmds();

    const DirectX::XMINT2 extents(execCtx.GetRenderResolution().x, execCtx.GetRenderResolution().y);

    RenderAttachmentInfo colorAttachment = execCtx.GetColorAttachment(RGResourceID::GBufferDebug, LoadOp::LOAD);
    RenderAttachmentInfo depthAttachment = execCtx.GetReadOnlyDepthAttachment(RGResourceID::MainDepth);

    BeginRenderingCmd cmdBegin{&m_pipeline, {colorAttachment}, depthAttachment};
    cmdBegin.extents = extents;
    cmdBegin.viewport = data.mainView.viewport;

    GenericIndirectDrawCmd cmd{&m_pipeline, cmdBuf};
    cmd.drawCount = 1;
    cmd.descriptorSets = execCtx.GetDescriptors();

    StartRenderPassProfilingScope(execCtx.pCmdBuffer);
    execCtx.pCmdBuffer->RecordCommand(cmdBegin);

    BinRenderDataCmd bindStateCmd(m_dummyVertexBuffer, m_indexBuffer);
    execCtx.pCmdBuffer->RecordCommand(bindStateCmd);

    execCtx.pCmdBuffer->RecordCommand(cmd);
    execCtx.pCmdBuffer->RecordCommand(EndRenderingCmd{});
    EndRenderPassProfilingScope(execCtx.pCmdBuffer);
}

bool ClusterDebugPass::WantsToRender() const
{
    return mathstl::isFlagSet(g_engine.GetApplicationState().GetCurrentApplicationState().renderState.debugFlags,
                              (u32)DebugFlags::ShowClusterAABBs);
}
