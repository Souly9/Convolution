#include "DLSSPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Rendering/Core/AntiAliasing.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Core/Rendering/Vulkan/VkTexture.h"
#include "Core/Rendering/Vulkan/Utils/VkEnumHelpers.h"
#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"
#include "Core/Rendering/Core/RenderGraph/RGExecutionContext.h"
#include <cstring>

using namespace RenderPasses;
using SL = Nvidia::StreamlineManager;

namespace
{
// Velocity holds current minus previous NDC (+y up), Streamline wants current -> previous in UV units
const mathstl::Vector2 kMotionVectorScale{-0.5f, 0.5f};

// Must match the layouts DLSSPass::Setup requests from the render graph
uint32_t GetTaggedLayout(sl::BufferType type)
{
    switch (type)
    {
        case sl::kBufferTypeScalingInputColor:
        case sl::kBufferTypeScalingOutputColor:
            return static_cast<uint32_t>(Conv(ImageLayout::GENERAL));
        case sl::kBufferTypeDepth:
            return static_cast<uint32_t>(Conv(ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL));
        default:
            return static_cast<uint32_t>(Conv(ImageLayout::SHADER_READ_ONLY_OPTIMAL));
    }
}

void CopyMatrixToStreamline(sl::float4x4& dst, const mathstl::Matrix& src)
{
    static_assert(sizeof(mathstl::Matrix) == sizeof(sl::float4x4), "mathstl::Matrix must match Streamline matrix ABI");
    std::memcpy(&dst, &src, sizeof(dst));
}

sl::float3 ToStreamline(mathstl::Vector3 v)
{
    v.Normalize();
    return {v.x, v.y, v.z};
}

sl::DLSSMode ResolveDLSSMode(u32 renderScalePercent)
{
    if (renderScalePercent >= 100)
        return sl::DLSSMode::eDLAA;
    if (renderScalePercent >= 75)
        return sl::DLSSMode::eMaxQuality;
    if (renderScalePercent >= 50)
        return sl::DLSSMode::eMaxPerformance;
    return sl::DLSSMode::eUltraPerformance;
}

sl::Resource MakeResource(Texture* pTex, sl::BufferType type)
{
    auto* pVkTex = static_cast<TextureVulkan*>(pTex);
    const bool valid = pVkTex && pVkTex->GetImage() != VK_NULL_HANDLE && pVkTex->GetImageView() != VK_NULL_HANDLE;
    sl::Resource res(sl::ResourceType::eTex2d,
                     valid ? reinterpret_cast<void*>(pVkTex->GetImage()) : nullptr,
                     nullptr,
                     valid ? reinterpret_cast<void*>(pVkTex->GetImageView()) : nullptr,
                     GetTaggedLayout(type));
    res.nativeFormat = static_cast<uint32_t>(VK_FORMAT_R8G8B8A8_UNORM);
    if (!valid)
        return res;

    const auto& info = pVkTex->GetInfo();
    res.width = info.extents.x;
    res.height = info.extents.y;
    res.nativeFormat = static_cast<uint32_t>(Conv(info.format));
    res.usage = Conv(info.usage);
    res.mipLevels = info.mipLevels > 0 ? info.mipLevels : 1u;
    res.arrayLayers = info.extents.z > 0 ? info.extents.z : 1u;
    res.flags = 0;
    return res;
}

sl::Constants BuildConstants(const ::SharedDataUBO& view, bool reset, const SL::DebugSettings& settings)
{
    sl::Constants constants{};
    // Streamline requires jitter free matrices, the jitter goes into jitterOffset
    CopyMatrixToStreamline(constants.cameraViewToClip, view.projection);
    CopyMatrixToStreamline(constants.clipToCameraView, view.projectionInverse);
    CopyMatrixToStreamline(constants.clipToLensClip, mathstl::Matrix::Identity);
    CopyMatrixToStreamline(constants.clipToPrevClip, view.clipToPrevClip);
    CopyMatrixToStreamline(constants.prevClipToClip, view.clipToPrevClip.Invert());

    // Same convention ApplyFrameJitter rasterizes with; the flips only exist for A/B testing
    constants.jitterOffset = {settings.flipJitterX ? -view.jitterOffset.x : view.jitterOffset.x,
                              settings.flipJitterY ? -view.jitterOffset.y : view.jitterOffset.y};
    const mathstl::Vector2 mvScale = settings.overrideMotionVectorScale ? settings.motionVectorScale : kMotionVectorScale;
    constants.mvecScale = {mvScale.x, mvScale.y};
    constants.cameraPinholeOffset = {0.0f, 0.0f};

    // Rows of the inverse view matrix are the camera basis in world space
    constants.cameraPos = {view.viewPos.x, view.viewPos.y, view.viewPos.z};
    constants.cameraRight = ToStreamline(view.viewInverse.Right());
    constants.cameraUp = ToStreamline(view.viewInverse.Up());
    constants.cameraFwd = ToStreamline(view.viewInverse.Forward());
    constants.cameraNear = view.zNear;
    constants.cameraFar = view.zFar;
    constants.cameraFOV = view.fovY;
    constants.cameraAspectRatio = view.aspectRatio;

    constants.motionVectorsInvalidValue = sl::INVALID_FLOAT;
    constants.depthInverted = sl::Boolean::eTrue;
    constants.cameraMotionIncluded = sl::Boolean::eTrue;
    constants.motionVectors3D = sl::Boolean::eFalse;
    constants.motionVectorsJittered = sl::Boolean::eFalse;
    constants.reset = reset ? sl::Boolean::eTrue : sl::Boolean::eFalse;
    return constants;
}

void TagAndEvaluate(VkCommandBuffer cmd,
                    const stltype::fixed_vector<sl::ResourceTag, 16, false>& tags,
                    const sl::FrameToken& frameToken,
                    Nvidia::DLSSVariant variant)
{
    const sl::Result tagRes = SL::SetTagForFrame(
        frameToken, sl::ViewportHandle(0), tags.data(), static_cast<u32>(tags.size()), (sl::CommandBuffer*)cmd);
    auto debugState = SL::GetDLSSDebugState();
    debugState.lastTagResult = tagRes;
    SL::SetDLSSDebugState(debugState);
    if (tagRes != sl::Result::eOk)
    {
        DEBUG_LOG_WARNF("[DLSSPass] slSetTagForFrame failed with result: 0x{:X}", static_cast<u32>(tagRes));
        return;
    }
    SL::Evaluate(variant, cmd, frameToken);
}
} // namespace

DLSSPass::DLSSPass() : ConvolutionRenderPass("DLSSPass")
{
}

DLSSPass::~DLSSPass()
{
}

void DLSSPass::Init(const SharedResourceManager& resourceManager)
{
}

bool DLSSPass::WantsToRender() const
{
    return AA::Current().IsDLSS();
}

void DLSSPass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("DLSSPass::Render");
    const AA::FrameConfig& aa = AA::Current();
    const bool rayReconstruction = aa.temporal == AA::Temporal::DLSSRR;
    const Nvidia::DLSSVariant variant =
        rayReconstruction ? Nvidia::DLSSVariant::RayReconstruction : Nvidia::DLSSVariant::SuperResolution;
    const SL::DebugSettings settings = SL::GetDebugSettings();
    const ::SharedDataUBO& view = *data.pViewData;
    CommandBuffer* pCmdBuffer = execCtx.pCmdBuffer;

    Texture* pColorIn = execCtx.GetTexture(RGResourceID::GBufferThisFrameColor);
    Texture* pColorOut = execCtx.GetTexture(RGResourceID::TemporalResolve);
    Texture* pDepth = execCtx.GetTexture(RGResourceID::MainDepth);
    Texture* pMotion = execCtx.GetTexture(RGResourceID::GBufferVelocity);
    if (!pColorIn || !pColorOut)
        return;

    StartRenderPassProfilingScope(pCmdBuffer);

    const auto outputExtents = pColorOut->GetInfo().extents;
    const auto inputExtents = pColorIn->GetInfo().extents;
    const sl::DLSSMode mode = ResolveDLSSMode(aa.renderScalePercent);

    const u32 frameSlot = ctx.currentFrame;
    sl::FrameToken* pFrameToken = nullptr;
    const bool canEvaluate =
        !settings.bypass && pDepth && pMotion && SL::GetFrameToken(frameSlot, pFrameToken) &&
        SL::EnsureConfigured(variant, outputExtents.x, outputExtents.y, mode, view.view, view.viewInverse);

    // Snapshot after EnsureConfigured, which publishes the config state
    auto debugState = SL::GetDLSSDebugState();
    debugState.variant = variant;
    debugState.inputWidth = inputExtents.x;
    debugState.inputHeight = inputExtents.y;
    debugState.outputWidth = outputExtents.x;
    debugState.outputHeight = outputExtents.y;

    sl::Result constRes = sl::Result::eOk;
    Texture* pExposure = nullptr;
    bool reset = false;
    if (canEvaluate)
    {
        // Filled by DLSSExposurePass earlier in the graph
        pExposure = settings.useExposureTexture ? execCtx.GetTexture(RGResourceID::DLSSExposure) : nullptr;

        TagStorage& storage = m_tags[frameSlot];
        storage.resources.clear();
        storage.tags.clear();
        const auto addTag = [&storage](Texture* pTex, sl::BufferType type)
        {
            storage.resources.push_back(MakeResource(pTex, type));
            sl::Resource& res = storage.resources.back();
            storage.tags.emplace_back(res.native ? &res : nullptr, type, sl::ResourceLifecycle::eValidUntilPresent);
        };
        addTag(pColorIn, sl::kBufferTypeScalingInputColor);
        addTag(pColorOut, sl::kBufferTypeScalingOutputColor);
        addTag(pDepth, sl::kBufferTypeDepth);
        addTag(pMotion, sl::kBufferTypeMotionVectors);
        if (pExposure)
            addTag(pExposure, sl::kBufferTypeExposure);
        if (rayReconstruction)
        {
            Texture* pAlbedo = execCtx.GetTexture(RGResourceID::GBufferAlbedo);
            Texture* pNoisyReflections = execCtx.GetTexture(RGResourceID::RTReflections);
            addTag(pAlbedo, sl::kBufferTypeAlbedo);
            addTag(pAlbedo, sl::kBufferTypeSpecularAlbedo);
            addTag(execCtx.GetTexture(RGResourceID::GBufferNormal), sl::kBufferTypeNormals);
            addTag(execCtx.GetTexture(RGResourceID::GBufferRoughness), sl::kBufferTypeRoughness);
            addTag(pNoisyReflections, sl::kBufferTypeSpecularHitNoisy);
            addTag(pNoisyReflections, sl::kBufferTypeDiffuseHitNoisy);
        }

        const bool uiReset = settings.resetGeneration != m_lastResetGeneration;
        // Always consume; a short-circuit would leave the reset set for the next frame
        const bool configReset = SL::ConsumeResetFlag(variant);
        reset = aa.temporalReset || !m_evaluatedLastFrame || uiReset || configReset;
        const sl::Constants constants = BuildConstants(view, reset, settings);
        constRes = SL::SetConstants(constants, *pFrameToken, sl::ViewportHandle(0));
        debugState.jitter = mathstl::Vector2(constants.jitterOffset.x, constants.jitterOffset.y);
        debugState.motionVectorScale = mathstl::Vector2(constants.mvecScale.x, constants.mvecScale.y);
        if (constRes != sl::Result::eOk)
            DEBUG_LOG_WARNF("[DLSSPass] slSetConstants failed with result: 0x{:X}", static_cast<u32>(constRes));
    }
    m_lastResetGeneration = settings.resetGeneration;

    const bool evaluate = canEvaluate && constRes == sl::Result::eOk;
    debugState.lastSetConstantsResult = constRes;
    debugState.bypassed = !evaluate;
    debugState.reset = reset;
    debugState.exposureTextureTagged = evaluate && pExposure != nullptr;
    SL::SetDLSSDebugState(debugState);

    if (evaluate)
    {
        ExecuteNativeCmd evaluateCmd{};
        const auto* pTags = &m_tags[frameSlot].tags;
        evaluateCmd.callback = [pTags, pFrameToken, variant](void* pNativeCmdBuf)
        { TagAndEvaluate(reinterpret_cast<VkCommandBuffer>(pNativeCmdBuf), *pTags, *pFrameToken, variant); };
        pCmdBuffer->RecordCommand(evaluateCmd);
    }
    else
    {
        // Both images are in GENERAL for this node; a smaller input lands in the top-left corner (no blit on compute)
        ImageToImageCopyCmd passthrough(pColorIn, pColorOut);
        passthrough.srcLayout = ImageLayout::GENERAL;
        passthrough.dstLayout = ImageLayout::GENERAL;
        pCmdBuffer->RecordCommand(passthrough);
    }
    m_evaluatedLastFrame = evaluate;

    EndRenderPassProfilingScope(pCmdBuffer);
}

void DLSSPass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    // GENERAL + transfer access so either DLSS or the bypass copy can run without extra barriers
    builder.ReadTexture(RGResourceID::GBufferThisFrameColor,
                        SyncStages::COMPUTE_SHADER | SyncStages::TRANSFER,
                        AccessFlags::SHADER_READ | AccessFlags::TRANSFER_READ,
                        ImageLayout::GENERAL);
    builder.ReadTexture(RGResourceID::MainDepth, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::GBufferVelocity, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::DLSSExposure, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    if (AA::Current().temporal == AA::Temporal::DLSSRR)
    {
        builder.ReadTexture(RGResourceID::GBufferNormal, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        builder.ReadTexture(RGResourceID::GBufferRoughness, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        builder.ReadTexture(RGResourceID::GBufferAlbedo, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        builder.ReadTexture(RGResourceID::RTReflections, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    }

    auto resolve = builder.DeclareStorageTexture(RGResourceID::TemporalResolve, TexFormat::R16G16B16A16_FLOAT, RGSizeClass::OutputResolution);
    builder.WriteStorageImage(resolve,
                              SyncStages::COMPUTE_SHADER | SyncStages::TRANSFER,
                              AccessFlags::SHADER_WRITE | AccessFlags::TRANSFER_WRITE);
    builder.SetHasSideEffects();
}

DLSSExposurePass::DLSSExposurePass() : ConvolutionRenderPass("DLSSExposurePass")
{
}

bool DLSSExposurePass::WantsToRender() const
{
    return AA::Current().IsDLSS() && SL::GetDebugSettings().useExposureTexture;
}

void DLSSExposurePass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    auto exposure = builder.DeclareStorageTexture(RGResourceID::DLSSExposure, TexFormat::R32_FLOAT, RGSizeClass::Fixed);
    builder.WriteStorageImage(exposure, SyncStages::TRANSFER, AccessFlags::TRANSFER_WRITE);
    builder.SetHasSideEffects();
}

void DLSSExposurePass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("DLSSExposurePass::Render");
    Texture* pExposure = execCtx.GetTexture(RGResourceID::DLSSExposure);
    if (!pExposure)
        return;

    // DLSS applies it the way the composite applies ubo.exposure before tonemapping
    const f32 exposure = g_engine.GetApplicationState().GetCurrentApplicationState().renderState.exposure;
    StagingBuffer& staging = m_staging[ctx.currentFrame];
    staging.EnsureCapacity(sizeof(f32));
    staging.CopyToMapped(&exposure, sizeof(exposure));

    ImageBufferCopyCmd copyExposure(&staging, pExposure);
    copyExposure.imageExtent = {1, 1, 1};
    copyExposure.dstLayout = ImageLayout::GENERAL;
    execCtx.pCmdBuffer->RecordCommand(copyExposure);
}
