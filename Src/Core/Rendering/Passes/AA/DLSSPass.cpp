#include "DLSSPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Core/Rendering/Core/Nvidia/StreamlineManager.h"
#include "Core/Rendering/Vulkan/VkTexture.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"
#include "Core/Rendering/Vulkan/Utils/VkEnumHelpers.h"
#include "sl.h"
#include "sl_dlss.h"
#include <cstring>

using namespace RenderPasses;
namespace
{
constexpr bool kForceStaticCameraDebug = false;

struct StreamlineTagDesc
{
    sl::BufferType type{};
    uint64_t native{};
    uint64_t view{};
    uint32_t width{};
    uint32_t height{};
    uint32_t nativeFormat{};
    uint32_t usage{};
    uint32_t state{};
    uint32_t mipLevels{1};
    uint32_t arrayLayers{1};
};

VkImageLayout GetTaggedLayout(sl::BufferType type)
{
    switch (type)
    {
        case sl::kBufferTypeScalingOutputColor:
            return VK_IMAGE_LAYOUT_GENERAL;
        case sl::kBufferTypeDepth:
            return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        case sl::kBufferTypeBackbuffer:
            return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        default:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
}

void CopyMatrixToStreamline(sl::float4x4& dst, const mathstl::Matrix& src)
{
    static_assert(sizeof(sl::float4x4) == sizeof(float) * 16, "Unexpected Streamline matrix size");
    static_assert(sizeof(mathstl::Matrix) == sizeof(sl::float4x4), "mathstl::Matrix must match Streamline matrix ABI");

    std::memcpy(&dst, &src, sizeof(dst));
}

mathstl::Vector3 GetNormalizedOrFallback(mathstl::Vector3 value, const mathstl::Vector3& fallback)
{
    if (value.LengthSquared() <= 0.0f)
        return fallback;
    value.Normalize();
    return value;
}

sl::DLSSMode ResolveDLSSModeForRenderScale(const mathstl::Vector2& renderResolution,
                                           const mathstl::Vector2& swapchainResolution)
{
    if (swapchainResolution.x <= 0.0f || renderResolution.x >= swapchainResolution.x)
        return sl::DLSSMode::eDLAA;

    const f32 upscalingPercentage = (renderResolution.x * 100.0f) / swapchainResolution.x;
    if (upscalingPercentage >= 75)
        return sl::DLSSMode::eMaxQuality;
    if (upscalingPercentage >= 50)
        return sl::DLSSMode::eMaxPerformance;
    return sl::DLSSMode::eUltraPerformance;
}
}

// ============================================================================
// DLSSPass Implementation
// ============================================================================

DLSSPass::DLSSPass() : ConvolutionRenderPass("DLSSPass")
{
}

DLSSPass::~DLSSPass()
{
}

void DLSSPass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
{
}

bool DLSSPass::WantsToRender() const
{
    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    const bool dlssSupported = Nvidia::StreamlineManager::IsDLSSSupported();
    if (!dlssSupported || renderState.aaType != AntialiasingType::DLSS)
    {
        m_wasActive = false;
        return false;
    }

    const bool useRayReconstruction = Nvidia::StreamlineManager::IsDLSSRRSupported() &&
                                      Nvidia::StreamlineManager::GetUseRayReconstructionThisFrame();

    const bool wantsToRender = !useRayReconstruction;
    if (!wantsToRender)
        m_wasActive = false;
    return wantsToRender;
}

void DLSSPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    ScopedZone("DLSSPass::Render");
    u32 frameIdx = ctx.imageIdx;
    
    sl::FrameToken* pFrameToken = nullptr;
    if (!Nvidia::StreamlineManager::GetFrameToken(frameIdx, pFrameToken))
        return;
    
    Texture* pColorIn = data.temporalResources.pCurrentColorTexture;

    Texture* pColorOut = data.temporalResources.pResolveTexture;
    Texture* pDepth = data.temporalResources.pCurrentDepthTexture;
    Texture* pMotion = data.pGbuffer->Get(GBufferTextureType::GBufferVelocity);
    Texture* pExposure = ctx.pDLSSExposureTexture;

    if (!pColorIn || !pColorOut || !pDepth || !pMotion || !pExposure || !ctx.pCurrentSwapchainTexture)
        return;

    const auto outputExtents = pColorOut->GetInfo().extents;
    const auto inputExtents = pColorIn->GetInfo().extents;

    const sl::DLSSMode dlssMode =
        ResolveDLSSModeForRenderScale(data.renderState.renderResolution, data.renderState.swapchainResolution);

    if (!Nvidia::StreamlineManager::EnsureDLSSConfigured(outputExtents.x, outputExtents.y, dlssMode))
        return;
    if (Nvidia::StreamlineManager::IsDLSSEvaluateBlocked())
        return;

    // Transition layouts for DLSS
    {
        const ImageLayout oldColorOutLayout = (data.renderState.recreatedThisFrame || !m_wasActive)
                                              ? ImageLayout::UNDEFINED
                                              : ImageLayout::SHADER_READ_ONLY_OPTIMAL;

        ImageLayoutTransitionCmd colorOutGen(pColorOut);
        colorOutGen.oldLayout = oldColorOutLayout;
        colorOutGen.newLayout = ImageLayout::GENERAL;
        VkTextureManager::SetLayoutBarrierMasks(colorOutGen, oldColorOutLayout, ImageLayout::GENERAL);
        pCmdBuffer->RecordCommand(colorOutGen);
    }
    const auto restoreColorOutReadLayout = [&]() {
        ImageLayoutTransitionCmd colorOutRead(pColorOut);
        colorOutRead.oldLayout = ImageLayout::GENERAL;
        colorOutRead.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
        VkTextureManager::SetLayoutBarrierMasks(colorOutRead, ImageLayout::GENERAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        pCmdBuffer->RecordCommand(colorOutRead);
    };

    sl::ViewportHandle viewport(0);

    stltype::fixed_vector<StreamlineTagDesc, 16> tagDescs;
    auto pushTagDesc = [&](Texture* pTex, sl::BufferType type) {
        if (!pTex)
        {
            StreamlineTagDesc desc{};
            desc.type = type;
            desc.native = 0;
            desc.view = 0;
            desc.state = static_cast<uint32_t>(GetTaggedLayout(type));
            tagDescs.push_back(desc);
            return true;
        }
        TextureVulkan* pVkTex = static_cast<TextureVulkan*>(pTex);
        StreamlineTagDesc desc{};
        desc.type = type;
        desc.native = reinterpret_cast<uint64_t>(pVkTex->GetImage());
        desc.view = reinterpret_cast<uint64_t>(pVkTex->GetImageView());
        desc.width = pVkTex->GetInfo().extents.x;
        desc.height = pVkTex->GetInfo().extents.y;
        desc.nativeFormat = static_cast<uint32_t>(Conv(pVkTex->GetInfo().format));
        desc.usage = Conv(pVkTex->GetInfo().usage);
        desc.state = static_cast<uint32_t>(GetTaggedLayout(type));
        desc.mipLevels = pVkTex->GetInfo().mipLevels > 0 ? pVkTex->GetInfo().mipLevels : 1u;
        desc.arrayLayers = pVkTex->GetInfo().extents.z > 0 ? pVkTex->GetInfo().extents.z : 1u;
        if (desc.nativeFormat == 0)
        {
            DEBUG_LOG_WARNF("[DLSSPass] Texture format is UNDEFINED for BufferType %d, Engine Format %d", static_cast<int>(type), static_cast<int>(pVkTex->GetInfo().format));
        }
        tagDescs.push_back(desc);
        return true;
    };

    bool tagsOk = pushTagDesc(pColorIn, sl::kBufferTypeScalingInputColor) &&
                  pushTagDesc(pColorOut, sl::kBufferTypeScalingOutputColor) &&
                  pushTagDesc(pDepth, sl::kBufferTypeDepth) &&
                  pushTagDesc(pMotion, sl::kBufferTypeMotionVectors) &&
                  pushTagDesc(pExposure, sl::kBufferTypeExposure);

    if (tagsOk)
    {
        pushTagDesc(nullptr, sl::kBufferTypeAlbedo);
        pushTagDesc(nullptr, sl::kBufferTypeSpecularAlbedo);
        pushTagDesc(nullptr, sl::kBufferTypeNormals);
        pushTagDesc(nullptr, sl::kBufferTypeRoughness);
        pushTagDesc(nullptr, sl::kBufferTypeSpecularHitNoisy);
    }
    else
    {
        restoreColorOutReadLayout();
        return;
    }

    // Camera constants
    sl::Constants slConst{};
    const auto& cameraData = ctx.cameraData;
    const mathstl::Matrix clipToPrevClip =
        kForceStaticCameraDebug ? mathstl::Matrix::Identity : cameraData.clipToPrevClip;
    const mathstl::Matrix prevClipToClip =
        kForceStaticCameraDebug ? mathstl::Matrix::Identity : cameraData.prevClipToClip;

    const mathstl::Vector2 streamlineJitter = cameraData.jitterOffset;

    mathstl::Matrix jitteredProj = cameraData.viewToClip;
    if (data.renderState.renderResolution.x > 0.0f && data.renderState.renderResolution.y > 0.0f)
    {
        jitteredProj.m[2][0] += streamlineJitter.x * 2.0f / data.renderState.renderResolution.x;
        jitteredProj.m[2][1] += streamlineJitter.y * -2.0f / data.renderState.renderResolution.y;
    }

    CopyMatrixToStreamline(slConst.cameraViewToClip, jitteredProj);
    CopyMatrixToStreamline(slConst.clipToCameraView, cameraData.clipToView);
    CopyMatrixToStreamline(slConst.clipToLensClip, mathstl::Matrix::Identity);
    CopyMatrixToStreamline(slConst.clipToPrevClip, clipToPrevClip);
    CopyMatrixToStreamline(slConst.prevClipToClip, prevClipToClip);

    slConst.jitterOffset = {streamlineJitter.x, streamlineJitter.y};
    slConst.mvecScale = {-1.0f, 1.0f};
    slConst.cameraPinholeOffset = {0.0f, 0.0f};
    const mathstl::Vector3 streamlineCameraUp =
        GetNormalizedOrFallback(cameraData.up, mathstl::Vector3::Up);
    const mathstl::Vector3 streamlineCameraRight =
        GetNormalizedOrFallback(cameraData.right, mathstl::Vector3::Right);
    const mathstl::Vector3 streamlineCameraForward =
        GetNormalizedOrFallback(cameraData.forward, mathstl::Vector3::Forward);
    slConst.cameraPos = {cameraData.position.x, cameraData.position.y, cameraData.position.z};
    slConst.cameraUp = {streamlineCameraUp.x, streamlineCameraUp.y, streamlineCameraUp.z};
    slConst.cameraRight = {streamlineCameraRight.x, streamlineCameraRight.y, streamlineCameraRight.z};
    slConst.cameraFwd = {streamlineCameraForward.x, streamlineCameraForward.y, streamlineCameraForward.z};
    slConst.cameraNear = cameraData.nearPlane;
    slConst.cameraFar = cameraData.farPlane;
    slConst.cameraFOV = cameraData.fovRadians;
    slConst.cameraAspectRatio = cameraData.aspectRatio;
    slConst.motionVectorsInvalidValue = sl::INVALID_FLOAT;
    slConst.depthInverted = sl::Boolean::eTrue;
    slConst.cameraMotionIncluded = sl::Boolean::eTrue;
    slConst.motionVectors3D = sl::Boolean::eFalse;
    const bool shouldReset =
        !m_wasActive || data.renderState.recreatedThisFrame || Nvidia::StreamlineManager::ConsumeDLSSResetFlag();
    slConst.reset = shouldReset ? sl::Boolean::eTrue : sl::Boolean::eFalse;
    slConst.motionVectorsJittered = sl::Boolean::eFalse;

    auto debugState = Nvidia::StreamlineManager::GetDLSSDebugState();
    debugState.streamlineInitialized = Nvidia::StreamlineManager::IsAvailable();
    debugState.inputWidth = inputExtents.x;
    debugState.inputHeight = inputExtents.y;
    debugState.outputWidth = outputExtents.x;
    debugState.outputHeight = outputExtents.y;
    debugState.jitter = streamlineJitter;
    debugState.motionVectorScale = {slConst.mvecScale.x, slConst.mvecScale.y};
    debugState.nearPlane = cameraData.nearPlane;
    debugState.farPlane = cameraData.farPlane;
    debugState.fovRadians = cameraData.fovRadians;
    debugState.aspectRatio = cameraData.aspectRatio;
    debugState.cameraMotionIncluded = slConst.cameraMotionIncluded == sl::Boolean::eTrue;
    debugState.motionVectorsJittered = slConst.motionVectorsJittered == sl::Boolean::eTrue;
    debugState.depthInverted = slConst.depthInverted == sl::Boolean::eTrue;
    debugState.lastSetConstantsResult = sl::Result::eOk;
    debugState.lastTagResult = sl::Result::eOk;
    debugState.lastEvaluateResult = sl::Result::eOk;

    sl::DLSSState dlssState{};
    if (Nvidia::StreamlineManager::GetDLSSState(dlssState))
    {
        debugState.estimatedVRAMUsageInBytes = dlssState.estimatedVRAMUsageInBytes;
    }

    Nvidia::StreamlineManager::SetDLSSDebugState(debugState);

    ExecuteNativeCmd streamlineCmd{};
    const u32 frameSlot = ctx.imageIdx;
    streamlineCmd.callback = [tagDescs, pFrameToken, frameSlot](void* pNativeCmdBuf) mutable {
        VkCommandBuffer cmd = reinterpret_cast<VkCommandBuffer>(pNativeCmdBuf);
        sl::ViewportHandle viewportHandle(0);
        if (!pFrameToken)
            return;

        static struct {
            stltype::fixed_vector<sl::Resource, 16> resources;
            stltype::fixed_vector<sl::ResourceTag, 16> tags;
        } s_slData[SWAPCHAIN_IMAGES];

        auto& frameData = s_slData[frameSlot];
        frameData.resources.clear();
        frameData.tags.clear();
        frameData.resources.reserve(tagDescs.size());
        frameData.tags.reserve(tagDescs.size());

        for (const auto& td : tagDescs)
        {
            if (td.native == 0 && td.view == 0)
            {
                frameData.resources.push_back(sl::Resource(
                    sl::ResourceType::eTex2d,
                    nullptr,
                    nullptr,
                    nullptr,
                    td.state
                ));
            }
            else
            {
                sl::Resource res(
                    sl::ResourceType::eTex2d,
                    reinterpret_cast<void*>(td.native),
                    nullptr,
                    reinterpret_cast<void*>(td.view),
                    td.state
                );
                res.width = td.width;
                res.height = td.height;
                res.nativeFormat = td.nativeFormat;
                res.usage = td.usage;
                res.mipLevels = td.mipLevels;
                res.arrayLayers = td.arrayLayers;
                res.flags = 0;

                frameData.resources.push_back(res);
            }
        }

        for (size_t i = 0; i < frameData.resources.size(); ++i)
        {
            const bool isNull = (tagDescs[i].native == 0 && tagDescs[i].view == 0);
            frameData.tags.emplace_back(isNull ? nullptr : &frameData.resources[i], tagDescs[i].type, sl::ResourceLifecycle::eValidUntilPresent);
        }

        const sl::Result tagRes =
            Nvidia::StreamlineManager::SetTagForFrame(*pFrameToken,
                                                      viewportHandle,
                                                      frameData.tags.data(),
                                                      static_cast<u32>(frameData.tags.size()),
                                                      (sl::CommandBuffer*)cmd);
        auto debugState = Nvidia::StreamlineManager::GetDLSSDebugState();
        debugState.lastTagResult = tagRes;
        Nvidia::StreamlineManager::SetDLSSDebugState(debugState);
        if (tagRes != sl::Result::eOk)
        {
            DEBUG_LOG_WARNF("[DLSSPass] slSetTagForFrame failed with result: 0x{:X}", static_cast<u32>(tagRes));
            return;
        }

        Nvidia::StreamlineManager::EvaluateDLSS(cmd, *pFrameToken);
    };

    StartRenderPassProfilingScope(pCmdBuffer);
    const sl::Result constRes = Nvidia::StreamlineManager::SetConstants(slConst, *pFrameToken, viewport);
    debugState = Nvidia::StreamlineManager::GetDLSSDebugState();
    debugState.lastSetConstantsResult = constRes;
    Nvidia::StreamlineManager::SetDLSSDebugState(debugState);
    if (constRes != sl::Result::eOk)
    {
        DEBUG_LOG_WARNF("[DLSSPass] slSetConstants failed with result: 0x{:X}", static_cast<u32>(constRes));
        EndRenderPassProfilingScope(pCmdBuffer);
        restoreColorOutReadLayout();
        return;
    }
    m_wasActive = true;
    pCmdBuffer->RecordCommand(streamlineCmd);
    EndRenderPassProfilingScope(pCmdBuffer);

    restoreColorOutReadLayout();
}
