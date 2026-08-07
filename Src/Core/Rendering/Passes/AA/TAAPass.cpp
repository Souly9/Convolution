#include "TAAPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/FrameGlobals.h"
#include "Core/Global/State/States.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Vulkan/Utils/VkDescriptorLayoutUtils.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Core/Global/Profiling.h"

using namespace RenderPasses;

TAAPass::TAAPass() : ConvolutionRenderPass("TAAPass")
{
    CreateSharedDescriptorLayout();
}

TAAPass::~TAAPass()
{
}

void TAAPass::Init(const SharedResourceManager& resourceManager)
{
    ScopedZone("TAAPass::Init");
    BuildPipelines();
}

void TAAPass::BuildPipelines()
{
    auto computeShader = Shader("Shaders/TAA.comp.spv", "main");

    ShaderCollection shaders{};
    shaders.pComputeShader = &computeShader;

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst;
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(TAAPushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    m_taaPipeline = ComputePipeline(shaders, pipeInfo);
}

void TAAPass::BuildBuffers()
{
}

bool TAAPass::WantsToRender() const
{
    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    return renderState.aaType == AntialiasingType::TAA_SMAA;
}

void TAAPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset(DescriptorPresets::Bindless(true));
    AppendLayoutPreset(DescriptorPresets::View());
    AppendLayoutPreset(DescriptorPresets::GBuffer());
}

void TAAPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                  FrameRendererContext& previousFrameCtx,
                                  u32 thisFrameNum)
{
}

#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"
#include "Core/Rendering/Core/RenderGraph/RGExecutionContext.h"

void TAAPass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.DeclareContexts<
        PassCtx::BindlessWithImages,
        PassCtx::View,
        PassCtx::GBufferCtx>();

    builder.ReadTexture(RGResourceID::GBufferThisFrameColor, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::MainDepth, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::GBufferVelocity, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    auto resolve = builder.DeclareStorageTexture(RGResourceID::TemporalResolve, TexFormat::R16G16B16A16_FLOAT, RGSizeClass::OutputResolution);
    builder.WriteStorageImage(resolve, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_WRITE);
    builder.SetHasSideEffects();
}

void TAAPass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("TAAPass::Render");
    CommandBuffer* pCmdBuffer = execCtx.pCmdBuffer;
    StartRenderPassProfilingScope(pCmdBuffer);

    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    const auto& currentAA = renderState.aaType;
    const u32 currentDebugMode = renderState.taaDebugMode;
    if (m_lastAAType != currentAA || m_lastDebugMode != currentDebugMode || data.renderState.recreatedThisFrame || renderState.taaSeedHistoryFromCurrentColor)
    {
        m_resetFramesRemaining = SWAPCHAIN_IMAGES;
        m_lastAAType = currentAA;
        m_lastDebugMode = currentDebugMode;
    }

    if (m_resetFramesRemaining > 0)
    {
        m_pushConstants.resetHistory = 1;
        m_resetFramesRemaining--;
    }
    else
    {
        m_pushConstants.resetHistory = 0;
    }

    m_pushConstants.frameIndex = execCtx.GetFrameIndex();
    m_pushConstants.resolutionX = execCtx.GetRenderResolution().x;
    m_pushConstants.resolutionY = execCtx.GetRenderResolution().y;
    m_pushConstants.outputResolutionX = execCtx.GetSwapchainResolution().x;
    m_pushConstants.outputResolutionY = execCtx.GetSwapchainResolution().y;
    m_pushConstants.zNear = execCtx.GetZNear();
    m_pushConstants.zFar = execCtx.GetZFar();
    m_pushConstants.currentJitterX = data.renderState.jitter.x;
    m_pushConstants.currentJitterY = data.renderState.jitter.y;
    m_pushConstants.previousJitterX = data.renderState.previousJitter.x;
    m_pushConstants.previousJitterY = data.renderState.previousJitter.y;
    m_pushConstants.velocityRejectionStart = renderState.taaVelocityRejectionStart;
    m_pushConstants.velocityRejectionEnd = renderState.taaVelocityRejectionEnd;
    m_pushConstants.debugMode = currentDebugMode;
    m_pushConstants.forceHistory = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::TAAForceHistory) ? 1u : 0u;

    u32 groupCountX = (static_cast<u32>(execCtx.GetSwapchainResolution().x) + 7) / 8;
    u32 groupCountY = (static_cast<u32>(execCtx.GetSwapchainResolution().y) + 7) / 8;
    u32 groupCountZ = 1;

    {
        GenericComputeDispatchCmd cmd(&m_taaPipeline, groupCountX, groupCountY, groupCountZ);
        cmd.descriptorSets = execCtx.GetDescriptors();
        cmd.SetPushConstants(0, m_pushConstants);
        pCmdBuffer->RecordCommand(cmd);
    }

    EndRenderPassProfilingScope(pCmdBuffer);
}
