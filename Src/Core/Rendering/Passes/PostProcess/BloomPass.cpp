#include "BloomPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/FrameGlobals.h"
#include "Core/Global/State/States.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Core/Global/Profiling.h"
#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"

using namespace RenderPasses;

BloomPass::BloomPass() : ConvolutionRenderPass("BloomPass")
{
    CreateSharedDescriptorLayout();
}

BloomPass::~BloomPass()
{
}

void BloomPass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
{
    ScopedZone("BloomPass::Init");
    BuildPipelines();
}

void BloomPass::BuildPipelines()
{
    auto downsampleShader = Shader("Shaders/BloomDownsample.comp.spv", "main");
    auto blurShader = Shader("Shaders/BloomBlur.comp.spv", "main");

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst;
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(BloomPushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    ShaderCollection downsampleShaders{};
    downsampleShaders.pComputeShader = &downsampleShader;
    m_downsamplePipeline = ComputePipeline(downsampleShaders, pipeInfo);

    ShaderCollection blurShaders{};
    blurShaders.pComputeShader = &blurShader;
    m_blurPipeline = ComputePipeline(blurShaders, pipeInfo);
}

bool BloomPass::WantsToRender() const
{
    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    return renderState.bloom.enabled;
}

void BloomPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalArrayTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalImages, 1));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GBufferUBO, 2));
}

void BloomPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    ScopedZone("BloomPass::Render");
    StartRenderPassProfilingScope(pCmdBuffer);

    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    if (!renderState.bloom.enabled || !data.pGbuffer)
    {
        EndRenderPassProfilingScope(pCmdBuffer);
        return;
    }

    const u32 fullWidth = static_cast<u32>(data.renderState.renderResolution.x);
    const u32 fullHeight = static_cast<u32>(data.renderState.renderResolution.y);
    const u32 bloomWidth = stltype::max(1u, fullWidth / 2u);
    const u32 bloomHeight = stltype::max(1u, fullHeight / 2u);

    const auto texArraySet = data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessTextureArray);
    const auto imageArraySet = data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessImageArray);
    const auto gbufferUBO = data.bufferDescriptors.at(UBO::DescriptorContentsType::GBuffer);

    const auto inputTexHandle = data.pGbuffer->GetHandle(GBufferTextureType::GBufferThisFrameColor);
    const auto downsampleHandle = data.pGbuffer->GetHandle(GBufferTextureType::BloomDownsample);
    const auto resultHandle = data.pGbuffer->GetHandle(GBufferTextureType::BloomResult);

    u32 groupCountX = (bloomWidth + 7u) / 8u;
    u32 groupCountY = (bloomHeight + 7u) / 8u;

    // Step 1: Downsample + Threshold pass (Full HDR -> BloomDownsample)
    {
        m_pushConstants.threshold = renderState.bloom.threshold;
        m_pushConstants.intensity = renderState.bloom.intensity;
        m_pushConstants.isVerticalPass = 0;
        m_pushConstants.width = fullWidth;
        m_pushConstants.height = fullHeight;
        m_pushConstants.outputWidth = bloomWidth;
        m_pushConstants.outputHeight = bloomHeight;
        m_pushConstants.inputTexIdx = inputTexHandle;
        m_pushConstants.outputImageIdx = downsampleHandle;

        GenericComputeDispatchCmd cmd(&m_downsamplePipeline, groupCountX, groupCountY, 1);
        cmd.descriptorSets = {texArraySet, imageArraySet, gbufferUBO};
        cmd.SetPushConstants(0, m_pushConstants);
        pCmdBuffer->RecordCommand(cmd);
    }

    // Step 2: Horizontal Blur (BloomDownsample -> BloomResult)
    {
        m_pushConstants.isVerticalPass = 0;
        m_pushConstants.width = bloomWidth;
        m_pushConstants.height = bloomHeight;
        m_pushConstants.outputWidth = bloomWidth;
        m_pushConstants.outputHeight = bloomHeight;
        m_pushConstants.inputTexIdx = downsampleHandle;
        m_pushConstants.outputImageIdx = resultHandle;

        GenericComputeDispatchCmd cmd(&m_blurPipeline, groupCountX, groupCountY, 1);
        cmd.descriptorSets = {texArraySet, imageArraySet, gbufferUBO};
        cmd.SetPushConstants(0, m_pushConstants);
        pCmdBuffer->RecordCommand(cmd);
    }

    // Step 3: Vertical Blur (BloomResult -> BloomDownsample)
    {
        m_pushConstants.isVerticalPass = 1;
        m_pushConstants.width = bloomWidth;
        m_pushConstants.height = bloomHeight;
        m_pushConstants.outputWidth = bloomWidth;
        m_pushConstants.outputHeight = bloomHeight;
        m_pushConstants.inputTexIdx = resultHandle;
        m_pushConstants.outputImageIdx = downsampleHandle;

        GenericComputeDispatchCmd cmd(&m_blurPipeline, groupCountX, groupCountY, 1);
        cmd.descriptorSets = {texArraySet, imageArraySet, gbufferUBO};
        cmd.SetPushConstants(0, m_pushConstants);
        pCmdBuffer->RecordCommand(cmd);
    }

    EndRenderPassProfilingScope(pCmdBuffer);
}

void BloomPass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.ReadTexture(RGResourceID::GBufferThisFrameColor, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    auto downsample = builder.DeclareStorageTexture(RGResourceID::BloomDownsample, TexFormat::R16G16B16A16_FLOAT, RGSizeClass::RenderResolution);
    auto result = builder.DeclareStorageTexture(RGResourceID::BloomResult, TexFormat::R16G16B16A16_FLOAT, RGSizeClass::RenderResolution);

    builder.WriteStorageImage(downsample, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_WRITE);
    builder.WriteStorageImage(result, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_WRITE);
    builder.SetHasSideEffects();
}
