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

DLSSPass::TagDesc MakeTag(Texture* pTex, sl::BufferType type)
{
    DLSSPass::TagDesc desc{};
    desc.type = type;
    desc.state = GetTaggedLayout(type);
    desc.nativeFormat = static_cast<uint32_t>(VK_FORMAT_R8G8B8A8_UNORM);

    auto* pVkTex = static_cast<TextureVulkan*>(pTex);
    if (!pVkTex || pVkTex->GetImage() == VK_NULL_HANDLE || pVkTex->GetImageView() == VK_NULL_HANDLE)
        return desc;

    const auto& info = pVkTex->GetInfo();
    desc.native = reinterpret_cast<uint64_t>(pVkTex->GetImage());
    desc.view = reinterpret_cast<uint64_t>(pVkTex->GetImageView());
    desc.width = info.extents.x;
    desc.height = info.extents.y;
    desc.nativeFormat = static_cast<uint32_t>(Conv(info.format));
    desc.usage = Conv(info.usage);
    desc.mipLevels = info.mipLevels > 0 ? info.mipLevels : 1u;
    desc.arrayLayers = info.extents.z > 0 ? info.extents.z : 1u;
    return desc;
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

struct StreamlineTagStorage
{
    stltype::fixed_vector<sl::Resource, 16, false> resources;
    stltype::fixed_vector<sl::ResourceTag, 16, false> tags;
};
StreamlineTagStorage s_tagStorage[FRAMES_IN_FLIGHT];

void TagAndEvaluate(VkCommandBuffer cmd,
                    const DLSSPass::TagList& tagDescs,
                    StreamlineTagStorage& storage,
                    const sl::FrameToken& frameToken,
                    Nvidia::DLSSVariant variant)
{
    storage.resources.clear();
    storage.tags.clear();
    for (const auto& td : tagDescs)
    {
        const bool isNull = td.native == 0 && td.view == 0;
        sl::Resource res(sl::ResourceType::eTex2d,
                         isNull ? nullptr : reinterpret_cast<void*>(td.native),
                         nullptr,
                         isNull ? nullptr : reinterpret_cast<void*>(td.view),
                         td.state);
        res.nativeFormat = td.nativeFormat;
        if (!isNull)
        {
            res.width = td.width;
            res.height = td.height;
            res.usage = td.usage;
            res.mipLevels = td.mipLevels;
            res.arrayLayers = td.arrayLayers;
            res.flags = 0;
        }
        storage.resources.push_back(res);
    }
    for (size_t i = 0; i < storage.resources.size(); ++i)
    {
        const bool isNull = tagDescs[i].native == 0 && tagDescs[i].view == 0;
        storage.tags.emplace_back(
            isNull ? nullptr : &storage.resources[i], tagDescs[i].type, sl::ResourceLifecycle::eValidUntilPresent);
    }

    const sl::Result tagRes = SL::SetTagForFrame(frameToken,
                                                 sl::ViewportHandle(0),
                                                 storage.tags.data(),
                                                 static_cast<u32>(storage.tags.size()),
                                                 (sl::CommandBuffer*)cmd);
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

    auto debugState = SL::GetDLSSDebugState();
    debugState.variant = variant;
    debugState.inputWidth = inputExtents.x;
    debugState.inputHeight = inputExtents.y;
    debugState.outputWidth = outputExtents.x;
    debugState.outputHeight = outputExtents.y;
    debugState.nearPlane = view.zNear;
    debugState.farPlane = view.zFar;
    debugState.fovRadians = view.fovY;
    debugState.aspectRatio = view.aspectRatio;

    const u32 frameSlot = ctx.currentFrame;
    sl::FrameToken* pFrameToken = nullptr;
    const bool canEvaluate = !settings.bypass && pDepth && pMotion && SL::GetFrameToken(frameSlot, pFrameToken) &&
                             SL::EnsureConfigured(variant, outputExtents.x, outputExtents.y, mode, view.view, view.viewInverse) &&
                             !SL::IsEvaluateBlocked(variant);

    sl::Result constRes = sl::Result::eOk;
    Texture* pExposure = nullptr;
    bool reset = false;
    if (canEvaluate)
    {
        // Filled by DLSSExposurePass earlier in the graph
        pExposure = settings.useExposureTexture ? execCtx.GetTexture(RGResourceID::DLSSExposure) : nullptr;

        TagList& tags = m_tags[frameSlot];
        tags.clear();
        tags.push_back(MakeTag(pColorIn, sl::kBufferTypeScalingInputColor));
        tags.push_back(MakeTag(pColorOut, sl::kBufferTypeScalingOutputColor));
        tags.push_back(MakeTag(pDepth, sl::kBufferTypeDepth));
        tags.push_back(MakeTag(pMotion, sl::kBufferTypeMotionVectors));
        if (pExposure)
            tags.push_back(MakeTag(pExposure, sl::kBufferTypeExposure));
        if (rayReconstruction)
        {
            Texture* pAlbedo = execCtx.GetTexture(RGResourceID::GBufferAlbedo);
            Texture* pNoisyReflections = execCtx.GetTexture(RGResourceID::RTReflections);
            tags.push_back(MakeTag(pAlbedo, sl::kBufferTypeAlbedo));
            tags.push_back(MakeTag(pAlbedo, sl::kBufferTypeSpecularAlbedo));
            tags.push_back(MakeTag(execCtx.GetTexture(RGResourceID::GBufferNormal), sl::kBufferTypeNormals));
            tags.push_back(MakeTag(execCtx.GetTexture(RGResourceID::GBufferRoughness), sl::kBufferTypeRoughness));
            tags.push_back(MakeTag(pNoisyReflections, sl::kBufferTypeSpecularHitNoisy));
            tags.push_back(MakeTag(pNoisyReflections, sl::kBufferTypeDiffuseHitNoisy));
        }

        const bool uiReset = settings.resetGeneration != m_lastResetGeneration;
        reset = aa.temporalReset || !m_evaluatedLastFrame || uiReset || SL::ConsumeResetFlag(variant);
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
        const TagList* pTags = &m_tags[frameSlot];
        evaluateCmd.callback = [pTags, pFrameToken, frameSlot, variant](void* pNativeCmdBuf)
        { TagAndEvaluate(reinterpret_cast<VkCommandBuffer>(pNativeCmdBuf), *pTags, s_tagStorage[frameSlot], *pFrameToken, variant); };
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
