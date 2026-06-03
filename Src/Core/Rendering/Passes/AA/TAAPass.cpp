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

void TAAPass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
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
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalArrayTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalImages, 1));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GBufferUBO, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ShadowmapUBO, 2));
}

void TAAPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                  FrameRendererContext& previousFrameCtx,
                                  u32 thisFrameNum)
{
}

void TAAPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    ScopedZone("TAAPass::Render");
    StartRenderPassProfilingScope(pCmdBuffer);

    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    const auto& currentAA = renderState.aaType;
    const u32 currentDebugMode = renderState.taaDebugMode;
    if (m_lastAAType != currentAA || m_lastDebugMode != currentDebugMode)
    {
        m_pushConstants.resetHistory = 1;
        m_lastAAType = currentAA;
        m_lastDebugMode = currentDebugMode;
    }
    else
    {
        m_pushConstants.resetHistory = 0;
    }

    m_pushConstants.frameIndex = ctx.currentFrame;
    m_pushConstants.resolutionX = data.renderState.renderResolution.x;
    m_pushConstants.resolutionY = data.renderState.renderResolution.y;
    m_pushConstants.outputResolutionX = data.renderState.swapchainResolution.x;
    m_pushConstants.outputResolutionY = data.renderState.swapchainResolution.y;
    m_pushConstants.zNear = ctx.zNear;
    m_pushConstants.zFar = ctx.zFar;
    m_pushConstants.currentJitterX = data.renderState.jitter.x;
    m_pushConstants.currentJitterY = data.renderState.jitter.y;
    m_pushConstants.previousJitterX = data.renderState.previousJitter.x;
    m_pushConstants.previousJitterY = data.renderState.previousJitter.y;
    m_pushConstants.velocityRejectionStart = renderState.taaVelocityRejectionStart;
    m_pushConstants.velocityRejectionEnd = renderState.taaVelocityRejectionEnd;
    m_pushConstants.debugMode = currentDebugMode;
    m_pushConstants.forceHistory = mathstl::isFlagSet(renderState.debugFlags, (u32)DebugFlags::TAAForceHistory) ? 1u : 0u;
    m_pushConstants.resetHistory |= data.renderState.recreatedThisFrame ? 1u : 0u;

    u32 groupCountX = (static_cast<u32>(data.renderState.swapchainResolution.x) + 7) / 8;
    u32 groupCountY = (static_cast<u32>(data.renderState.swapchainResolution.y) + 7) / 8;
    u32 groupCountZ = 1;

    {
        GenericComputeDispatchCmd cmd(&m_taaPipeline, groupCountX, groupCountY, groupCountZ);
        const auto texArraySet = data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessTextureArray);
        const auto imageArraySet = data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessImageArray);
        const auto gbufferUBO = data.bufferDescriptors.at(UBO::DescriptorContentsType::GBuffer);
        
        cmd.descriptorSets = {texArraySet,
                              imageArraySet,
                              gbufferUBO};
        cmd.SetPushConstants(0, m_pushConstants);
        pCmdBuffer->RecordCommand(cmd);
    }

    EndRenderPassProfilingScope(pCmdBuffer);
}
