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
#include "Core/Rendering/Core/AntiAliasing.h"

using namespace RenderPasses;

BloomPass::BloomPass() : ConvolutionRenderPass("BloomPass")
{
    CreateSharedDescriptorLayout();
}

BloomPass::~BloomPass()
{
}

void BloomPass::Init(const SharedResourceManager& resourceManager)
{
    ScopedZone("BloomPass::Init");
    BuildPipelines();

    TextureManager::TexCreateInfo lens1Info("Resources/Bloom/lens_flare_1.png", true, TextureSemantic::Auto, true);
    TextureManager::TexCreateInfo lens2Info("Resources/Bloom/lens_flare_2.png", true, TextureSemantic::Auto, true);

    m_hLens1 = g_pTexManager->SubmitAsyncTextureCreation(lens1Info);
    m_hLens2 = g_pTexManager->SubmitAsyncTextureCreation(lens2Info);
}

void BloomPass::BuildPipelines()
{
    auto downsampleShader = Shader("Shaders/BloomDownsample.comp.spv", "main");
    auto upsampleShader = Shader("Shaders/BloomUpsample.comp.spv", "main");

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

    ShaderCollection upsampleShaders{};
    upsampleShaders.pComputeShader = &upsampleShader;
    m_upsamplePipeline = ComputePipeline(upsampleShaders, pipeInfo);
}

bool BloomPass::WantsToRender() const
{
    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    return renderState.bloom.enabled;
}

void BloomPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset(DescriptorPresets::Bindless(true));
    AppendLayoutPreset(DescriptorPresets::View());
    AppendLayoutPreset(DescriptorPresets::GBuffer());
}

void BloomPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
}

#include "Core/Rendering/Core/RenderGraph/RGExecutionContext.h"

void BloomPass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("BloomPass::Render");
    StartRenderPassProfilingScope(execCtx.pCmdBuffer);

    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    if (!renderState.bloom.enabled || execCtx.pRegistry == nullptr)
    {
        EndRenderPassProfilingScope(execCtx.pCmdBuffer);
        return;
    }

    static const RGResourceID bloomResIDs[5] = {
        RGResourceID::BloomMip0,
        RGResourceID::BloomMip1,
        RGResourceID::BloomMip2,
        RGResourceID::BloomMip3,
        RGResourceID::BloomMip4
    };

    const RGResourceID bloomInputID = AA::Current().aaOutput;
    const Texture* pInputTex = execCtx.GetTexture(bloomInputID);

    struct MipDimension
    {
        u32 width;
        u32 height;
    };
    MipDimension mips[5];
    u32 currW = static_cast<u32>(data.renderState.renderResolution.x);
    u32 currH = static_cast<u32>(data.renderState.renderResolution.y);
    for (u32 i = 0; i < 5; ++i)
    {
        mips[i] = {currW, currH};
        currW = stltype::max(1u, currW / 2u);
        currH = stltype::max(1u, currH / 2u);
    }

    // ------------------------------------------------------------------------
    // Step 1: Progressive Downsample Chain (Jimenez 13-Tap Filter)
    // ------------------------------------------------------------------------
    for (u32 i = 0; i < 5; ++i)
    {
        const u32 srcW = (i == 0) ? static_cast<u32>(pInputTex ? pInputTex->GetInfo().extents.x : execCtx.GetRenderResolution().x) : mips[i - 1].width;
        const u32 srcH = (i == 0) ? static_cast<u32>(pInputTex ? pInputTex->GetInfo().extents.y : execCtx.GetRenderResolution().y) : mips[i - 1].height;
        const u32 dstW = mips[i].width;
        const u32 dstH = mips[i].height;

        BindlessTextureHandle srcTexHandle = (i == 0) 
            ? execCtx.GetBindless(bloomInputID)
            : execCtx.GetBindless(bloomResIDs[i - 1]);

        BindlessTextureHandle dstImgHandle = execCtx.GetBindless(bloomResIDs[i]);

        m_pushConstants.threshold = renderState.bloom.threshold;
        m_pushConstants.intensity = renderState.bloom.intensity;
        m_pushConstants.filterRadius = 1.0f;
        m_pushConstants.useKarisAverage = (i == 0) ? 1u : 0u;
        m_pushConstants.width = srcW;
        m_pushConstants.height = srcH;
        m_pushConstants.outputWidth = dstW;
        m_pushConstants.outputHeight = dstH;
        m_pushConstants.inputTexIdx = srcTexHandle;
        m_pushConstants.inputTargetTexIdx = 0;
        m_pushConstants.outputImageIdx = dstImgHandle;
        m_pushConstants.useLensTexture = 0u;
        m_pushConstants.lensTextureIdx = 0;
        m_pushConstants.lensDirtIntensity = 0.0f;

        u32 groupX = (dstW + 7u) / 8u;
        u32 groupY = (dstH + 7u) / 8u;

        GenericComputeDispatchCmd cmd(&m_downsamplePipeline, groupX, groupY, 1);
        cmd.descriptorSets = execCtx.GetDescriptors();
        cmd.SetPushConstants(0, m_pushConstants);
        execCtx.pCmdBuffer->RecordCommand(cmd);
        execCtx.pCmdBuffer->RecordCommand(GlobalBarrierCmd(
            SyncStages::COMPUTE_SHADER, SyncStages::COMPUTE_SHADER,
            AccessFlags::SHADER_WRITE, AccessFlags::SHADER_READ | AccessFlags::SHADER_WRITE));
    }

    // ------------------------------------------------------------------------
    // Step 2: Progressive Upsample & Additive Accumulation (9-Tap Tent Filter)
    // ------------------------------------------------------------------------
    for (s32 i = 3; i >= 0; --i)
    {
        const u32 srcW = mips[i + 1].width;
        const u32 srcH = mips[i + 1].height;
        const u32 dstW = mips[i].width;
        const u32 dstH = mips[i].height;

        BindlessTextureHandle lowerMipTexHandle = execCtx.GetBindless(bloomResIDs[i + 1]);
        BindlessTextureHandle targetMipTexHandle = execCtx.GetBindless(bloomResIDs[i]);
        BindlessTextureHandle targetMipImgHandle = execCtx.GetBindless(bloomResIDs[i]);

        m_pushConstants.threshold = renderState.bloom.threshold;
        m_pushConstants.intensity = renderState.bloom.intensity;
        m_pushConstants.filterRadius = 1.0f;
        m_pushConstants.useKarisAverage = (i == 0) ? 1u : 0u;
        m_pushConstants.width = srcW;
        m_pushConstants.height = srcH;
        m_pushConstants.outputWidth = dstW;
        m_pushConstants.outputHeight = dstH;
        m_pushConstants.inputTexIdx = lowerMipTexHandle;
        m_pushConstants.inputTargetTexIdx = targetMipTexHandle;
        m_pushConstants.outputImageIdx = targetMipImgHandle;
        m_pushConstants.useLensTexture = 0u;
        m_pushConstants.lensTextureIdx = 0;
        m_pushConstants.lensDirtIntensity = renderState.bloom.lensDirtIntensity;

        if (i == 0 && renderState.bloom.lensTextureIndex > 0)
        {
            TextureHandle targetLensHandle = (renderState.bloom.lensTextureIndex == 1) ? m_hLens1 : m_hLens2;
            auto* pLensTex = g_pTexManager->GetTexture(targetLensHandle);
            if (pLensTex)
            {
                BindlessTextureHandle bindlessIdx = g_pTexManager->MakeTextureBindless(pLensTex, true);
                m_pushConstants.useLensTexture = 1u;
                m_pushConstants.lensTextureIdx = bindlessIdx;
            }
        }

        u32 groupX = (dstW + 7u) / 8u;
        u32 groupY = (dstH + 7u) / 8u;

        GenericComputeDispatchCmd cmd(&m_upsamplePipeline, groupX, groupY, 1);
        cmd.descriptorSets = execCtx.GetDescriptors();
        cmd.SetPushConstants(0, m_pushConstants);
        execCtx.pCmdBuffer->RecordCommand(cmd);
        execCtx.pCmdBuffer->RecordCommand(GlobalBarrierCmd(
            SyncStages::COMPUTE_SHADER, SyncStages::COMPUTE_SHADER,
            AccessFlags::SHADER_WRITE, AccessFlags::SHADER_READ | AccessFlags::SHADER_WRITE));
    }

    EndRenderPassProfilingScope(execCtx.pCmdBuffer);
}

void BloomPass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.DeclareContexts<
        PassCtx::BindlessWithImages,
        PassCtx::View,
        PassCtx::GBufferCtx>();

    // Bloom works on the resolved image so it inherits the AA
    builder.ReadTexture(AA::Current().aaOutput, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    static const RGResourceID bloomResIDs[5] = {
        RGResourceID::BloomMip0,
        RGResourceID::BloomMip1,
        RGResourceID::BloomMip2,
        RGResourceID::BloomMip3,
        RGResourceID::BloomMip4
    };

    for (u32 i = 0; i < 5; ++i)
    {
        auto mip = builder.DeclareStorageTexture(bloomResIDs[i], TexFormat::R16G16B16A16_FLOAT, RGSizeClass::RenderResolution);
        builder.WriteStorageImage(mip, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ | AccessFlags::SHADER_WRITE);
    }

    builder.SetHasSideEffects();
}
