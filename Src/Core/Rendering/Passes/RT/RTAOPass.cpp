#include "RTAOPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"
#include "Core/Global/Profiling.h"
#include "Core/Global/Utils/MathFunctions.h"

using namespace RenderPasses;

RTAOPass::RTAOPass() : RTComputePassBase("RTAOPass")
{
    CreateSharedDescriptorLayout();
}

void RTAOPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset(DescriptorPresets::Bindless(true));
    AppendLayoutPreset(DescriptorPresets::View());
    AppendLayoutPreset(DescriptorPresets::GlobalInstanceData());
    AppendLayoutPreset(DescriptorPresets::GBuffer());
    AppendLayoutPreset(DescriptorPresets::LightCluster());
    AppendLayoutPreset(DescriptorPresets::RTScene());
}

void RTAOPass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
{
    ScopedZone("RTAOPass::Init");
    (void)attachmentInfo;
    (void)resourceManager;
    CreateTLASDescriptorResources(true);
    BuildPipelines();
}

void RTAOPass::BuildPipelines()
{
    auto computeShader = Shader("Shaders/RTAO.comp.spv", "main");

    ShaderCollection shaders{};
    shaders.pComputeShader = &computeShader;

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst{};
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(RTAOPushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    m_computePipeline = ComputePipeline(shaders, pipeInfo);
}

void RTAOPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                   FrameRendererContext& previousFrameCtx,
                                   u32 thisFrameNum)
{
    (void)meshes;
    (void)previousFrameCtx;
    (void)thisFrameNum;
}

bool RTAOPass::WantsToRender() const
{
    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    return mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTEnabled) &&
           mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTAOEnabled);
}

void RTAOPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    if (data.pRTSceneManager == nullptr ||
        data.pResourceManager == nullptr ||
        data.rtaoTextureHandle == 0)
    {
        return;
    }

    const auto& sceneGeometryBuffers = data.pResourceManager->GetSceneGeometryBuffers();
    if (!sceneGeometryBuffers.GetVertexBuffer().IsCreated() || !sceneGeometryBuffers.GetIndexBuffer().IsCreated())
    {
        return;
    }

    StartRenderPassProfilingScope(pCmdBuffer);

    const bool hasReadyTLAS = data.pRTSceneManager->HasReadyTLAS(ctx.imageIdx);
    if (!hasReadyTLAS)
    {
        EndRenderPassProfilingScope(pCmdBuffer);
        return;
    }

    {
        const RT::TLASFrameData* pTLASFrameData = data.pRTSceneManager->GetTLASFrameData(ctx.imageIdx);
        if (pTLASFrameData == nullptr || !pTLASFrameData->hitDataBuffer.IsCreated())
        {
            EndRenderPassProfilingScope(pCmdBuffer);
            return;
        }
        else
        {
            m_tlasDescriptors[ctx.imageIdx]->WriteAccelerationStructureUpdate(pTLASFrameData->accelerationStructure,
                                                                              s_rtSceneASBindingSlot);
            m_tlasDescriptors[ctx.imageIdx]->WriteSSBOUpdate(pTLASFrameData->hitDataBuffer,
                                                             s_rtInstanceHitDataBindingSlot);
            m_tlasDescriptors[ctx.imageIdx]->WriteSSBOUpdate(sceneGeometryBuffers.GetVertexBuffer(),
                                                             s_rtSceneVertexBufferBindingSlot);
            m_tlasDescriptors[ctx.imageIdx]->WriteSSBOUpdate(sceneGeometryBuffers.GetIndexBuffer(),
                                                             s_rtSceneIndexBufferBindingSlot);
        }
    }

    const auto& rtState = g_pApplicationState->GetCurrentApplicationState().renderState.rt;
    m_pushConstants.rtaoTexIdx = data.rtaoTextureHandle;
    m_pushConstants.hasReadyTLAS = hasReadyTLAS ? 1u : 0u;
    m_pushConstants.frameIndex = ctx.currentFrame;
    m_pushConstants.raysPerPixel = rtState.aoRaysPerPixel;
    m_pushConstants.aoRadius = rtState.aoRadius;
    m_pushConstants.aoIntensity = rtState.aoIntensity;

    const u32 groupCountX = (static_cast<u32>(data.renderState.renderResolution.x) + 7) / 8;
    const u32 groupCountY = (static_cast<u32>(data.renderState.renderResolution.y) + 7) / 8;

    GenericComputeDispatchCmd dispatchCmd(&m_computePipeline, groupCountX, groupCountY, 1);
    dispatchCmd.descriptorSets = {g_pTexManager->GetCombinedBindlessDescriptorSet(),
                                  data.mainView.descriptorSet,
                                  data.bufferDescriptors.at(UBO::DescriptorContentsType::GlobalInstanceData),
                                  data.bufferDescriptors.at(UBO::DescriptorContentsType::GBuffer),
                                  data.bufferDescriptors.at(UBO::DescriptorContentsType::LightData),
                                  m_tlasDescriptors[ctx.imageIdx]};
    dispatchCmd.SetPushConstants(0, m_pushConstants);
    pCmdBuffer->RecordCommand(dispatchCmd);

    EndRenderPassProfilingScope(pCmdBuffer);
}
