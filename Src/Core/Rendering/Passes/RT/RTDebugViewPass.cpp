#include "RTDebugViewPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include "Core/Rendering/Core/RT/RTSceneManager.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"

using namespace RenderPasses;

RTDebugViewPass::RTDebugViewPass() : RTComputePassBase("RTDebugViewPass")
{
    CreateSharedDescriptorLayout();
}

void RTDebugViewPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset(DescriptorPresets::Bindless(true));
    AppendLayoutPreset(DescriptorPresets::View());
    AppendLayoutPreset(DescriptorPresets::GlobalInstanceData());
    AppendLayoutPreset(DescriptorPresets::GBuffer());
    AppendLayoutPreset(DescriptorPresets::LightCluster());
    AppendLayoutPreset(DescriptorPresets::RTScene(false));
}

void RTDebugViewPass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
{
    ScopedZone("RTDebugViewPass::Init");
    (void)attachmentInfo;
    (void)resourceManager;
    CreateTLASDescriptorResources(false);
    BuildPipelines();
}

void RTDebugViewPass::BuildPipelines()
{
    auto computeShader = Shader("Shaders/RTDebugView.comp.spv", "main");

    ShaderCollection shaders{};
    shaders.pComputeShader = &computeShader;

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst{};
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(RTDebugViewPushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    m_computePipeline = ComputePipeline(shaders, pipeInfo);
}

void RTDebugViewPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                          FrameRendererContext& previousFrameCtx,
                                          u32 thisFrameNum)
{
    (void)meshes;
    (void)previousFrameCtx;
    (void)thisFrameNum;
}

bool RTDebugViewPass::WantsToRender() const
{
    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    return mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTEnabled) &&
           mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTDebugEnabled);
}

void RTDebugViewPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    if (data.pRTSceneManager == nullptr || data.rtDebugTextureHandle == 0)
        return;

    StartRenderPassProfilingScope(pCmdBuffer);

    const bool hasReadyTLAS = data.pRTSceneManager->HasReadyTLAS(ctx.imageIdx);
    if (!hasReadyTLAS)
    {
        EndRenderPassProfilingScope(pCmdBuffer);
        return;
    }

    const RT::TLASFrameData* pTLASFrameData = data.pRTSceneManager->GetTLASFrameData(ctx.imageIdx);
    if (pTLASFrameData == nullptr)
    {
        EndRenderPassProfilingScope(pCmdBuffer);
        return;
    }

    m_tlasDescriptors[ctx.currentFrame]->WriteAccelerationStructureUpdate(pTLASFrameData->accelerationStructure,
                                                                      s_rtSceneASBindingSlot);

    const auto& rtState = g_pApplicationState->GetCurrentApplicationState().renderState.rt;
    m_pushConstants.outputTexIdx = data.rtDebugTextureHandle;
    m_pushConstants.debugMode = rtState.debugMode;
    m_pushConstants.maxRayDistance = ctx.zFar;

    const u32 groupCountX = (static_cast<u32>(data.renderState.renderResolution.x) + 7) / 8;
    const u32 groupCountY = (static_cast<u32>(data.renderState.renderResolution.y) + 7) / 8;

    GenericComputeDispatchCmd dispatchCmd(&m_computePipeline, groupCountX, groupCountY, 1);
    dispatchCmd.descriptorSets = {g_pTexManager->GetCombinedBindlessDescriptorSet(),
                                  data.mainView.descriptorSet,
                                  data.bufferDescriptors.at(UBO::DescriptorContentsType::GlobalInstanceData),
                                  data.bufferDescriptors.at(UBO::DescriptorContentsType::GBuffer),
                                  data.bufferDescriptors.at(UBO::DescriptorContentsType::LightData),
                                  m_tlasDescriptors[ctx.currentFrame]};
    dispatchCmd.SetPushConstants(0, m_pushConstants);
    pCmdBuffer->RecordCommand(dispatchCmd);
    EndRenderPassProfilingScope(pCmdBuffer);
}
