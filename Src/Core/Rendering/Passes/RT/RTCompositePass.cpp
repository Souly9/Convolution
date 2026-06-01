#include "RTCompositePass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/View.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/Global/Profiling.h"

using namespace RenderPasses;

RTCompositePass::RTCompositePass() : GenericGeometryPass("RTCompositePass")
{
    SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates::Complete);
    CreateSharedDescriptorLayout();
}

void RTCompositePass::Init(RendererAttachmentInfo& attachmentInfo,
                                       const SharedResourceManager& resourceManager)
{
    ScopedZone("RTCompositePass::Init");

    RecreateResolutionDependentResources(attachmentInfo, resourceManager);
    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
        m_indirectCmdBuffers[i].Init(10);
    BuildPipelines();
}

void RTCompositePass::RecreateResolutionDependentResources(RendererAttachmentInfo& attachmentInfo,
                                                         const SharedResourceManager& resourceManager)
{
    ScopedZone("RTCompositePass::RecreateResolutionDependentResources");

    const auto rtCompositeAttachment =
        CreateDefaultColorAttachment(TexFormat::R16G16B16A16_FLOAT, LoadOp::CLEAR, nullptr);
    m_mainRenderingData.colorAttachments = {rtCompositeAttachment};

    InitBaseData(attachmentInfo);
}

void RTCompositePass::BuildPipelines()
{
    ScopedZone("RTCompositePass::BuildPipelines");
    auto mainVert = Shader("Shaders/CompositPass.vert.spv", "main");
    auto mainFrag = Shader("Shaders/RTComposite.frag.spv", "main");

    PipelineInfo info{};
    info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
    info.attachmentInfos = CreateAttachmentInfo({m_mainRenderingData.colorAttachments});
    info.hasDepth = false;
    m_mainPSO = PSO(
        ShaderCollection{&mainVert, &mainFrag}, PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions}, info);
}

void RTCompositePass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
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

void RTCompositePass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    const auto currentFrame = ctx.imageIdx;
    UpdateContextForFrame(currentFrame);

    ColorAttachment sceneCompositeAttachment = m_mainRenderingData.colorAttachments[0];
    sceneCompositeAttachment.SetTexture(data.pRTReflectedSceneColorTexture);

    stltype::vector<ColorAttachment> colorAttachments = {sceneCompositeAttachment};

    const auto ex = data.pRTReflectedSceneColorTexture->GetInfo().extents;
    const DirectX::XMINT2 extents(ex.x, ex.y);

    BeginRenderingCmd cmdBegin{&m_mainPSO, ToRenderAttachmentInfos(colorAttachments)};
    cmdBegin.extents = extents;
    cmdBegin.viewport = RenderViewUtils::CreateViewportFromData(data.renderState.renderResolution, ctx.zNear, ctx.zFar);

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

void RTCompositePass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset(DescriptorPresets::Bindless());
    AppendLayoutPreset(DescriptorPresets::View());
    AppendLayoutPreset(DescriptorPresets::GlobalInstanceData());
    AppendLayoutPreset(DescriptorPresets::GBuffer());
}

bool RTCompositePass::WantsToRender() const
{
    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    const bool reflectionsEnabled = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTEnabled) &&
                                    mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTReflectionsEnabled);
    const bool rtaoEnabled = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTEnabled) &&
                             mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTAOEnabled);
    return reflectionsEnabled || rtaoEnabled;
}
