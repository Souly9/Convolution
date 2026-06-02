#include "RTCompositePass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/View.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"

using namespace RenderPasses;

RTCompositePass::RTCompositePass() : ConvolutionRenderPass("RTCompositePass")
{
    CreateSharedDescriptorLayout();
}

void RTCompositePass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
{
    (void)attachmentInfo;
    (void)resourceManager;
    BuildPipelines();
}

void RTCompositePass::BuildPipelines()
{
    auto computeShader = Shader("Shaders/RTComposite.comp.spv", "main");

    ShaderCollection shaders{};
    shaders.pComputeShader = &computeShader;

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst;
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(RTCompositePushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    m_pipeline = ComputePipeline(shaders, pipeInfo);
}

void RTCompositePass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    ScopedZone("RTCompositePass::Render");
    StartRenderPassProfilingScope(pCmdBuffer);

    const bool shouldReset = !m_wasActive || data.renderState.recreatedThisFrame;
    m_wasActive = true;

    if (shouldReset)
        m_accumFrameCount = 0;
    else
        m_accumFrameCount = eastl::min(m_accumFrameCount + 1u, 8u);

    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    const bool rtaoEnabled = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTEnabled) &&
                             mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTAOEnabled);
    const bool reflectionsEnabled = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTEnabled) &&
                                    mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::RTReflectionsEnabled);

    m_pushConstants.resetHistory        = shouldReset ? 1u : 0u;
    m_pushConstants.accumRate           = 1.0f / static_cast<float>(m_accumFrameCount + 1);
    m_pushConstants.accumTexIdx         = data.rtAccumulationTextureHandle;
    m_pushConstants.rtaoTexIdx          = rtaoEnabled ? data.rtaoTextureHandle : 0u;
    m_pushConstants.rtReflectionsTexIdx = reflectionsEnabled ? data.rtReflectionsTextureHandle : 0u;

    const u32 groupCountX = (static_cast<u32>(data.renderState.renderResolution.x) + 7) / 8;
    const u32 groupCountY = (static_cast<u32>(data.renderState.renderResolution.y) + 7) / 8;
    const u32 groupCountZ = 1;

    GenericComputeDispatchCmd cmd(&m_pipeline, groupCountX, groupCountY, groupCountZ);
    if (!data.bufferDescriptors.empty())
    {
        const auto gbufferUBOSet = data.bufferDescriptors.at(UBO::DescriptorContentsType::GBuffer);

        cmd.descriptorSets = {g_pTexManager->GetCombinedBindlessDescriptorSet(),
                              data.mainView.descriptorSet,
                              data.bufferDescriptors.at(UBO::DescriptorContentsType::GlobalInstanceData),
                              gbufferUBOSet};
        cmd.SetPushConstants(0, m_pushConstants);
    }
    pCmdBuffer->RecordCommand(cmd);

    EndRenderPassProfilingScope(pCmdBuffer);
}

void RTCompositePass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset(DescriptorPresets::Bindless(true)); // Bindless with images
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
    const bool wantsToRender = reflectionsEnabled || rtaoEnabled;
    if (!wantsToRender)
    {
        m_wasActive = false;
    }
    return wantsToRender;
}
