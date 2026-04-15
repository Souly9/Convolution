#include "DLSSPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/FrameGlobals.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Core/Rendering/Core/Nvidia/StreamlineManager.h"
#include "Core/Rendering/Vulkan/VkTexture.h"
#include "Core/Rendering/Vulkan/VkBuffer.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"
#include "Core/Rendering/Vulkan/Utils/VkEnumHelpers.h"
#include "Core/Rendering/Core/Utils/TAA/JitterFunctions.h"
#include "sl.h"
#include "sl_dlss.h"
#include <iostream>

using namespace RenderPasses;
namespace
{
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
        default:
            return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
}
}

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
    return renderState.aaType == AntialiasingType::DLSS;
}

void DLSSPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    ScopedZone("DLSSPass::Render");
    u32 frameIdx = FrameGlobals::GetFrameNumber();
    
    sl::FrameToken* pFrameToken = nullptr;
    if (!Nvidia::StreamlineManager::GetFrameToken(frameIdx, pFrameToken))
        return;
    
    // Need: Color (In/Out), Depth, Motion Vectors
    // In DLSS mode TAA is disabled, so GBufferResolve is not a valid input source.
    // Feed DLSS with the lit current frame color and write the DLSS result to resolve.
    Texture* pColorIn = data.pGbuffer->Get(GBufferTextureType::GBufferThisFrameColor);
    Texture* pColorOut = data.pGbuffer->Get(GBufferTextureType::GBufferResolve);
    Texture* pDepth = data.pMainDepthTexture; // Current Depth
    Texture* pMotion = data.pGbuffer->Get(GBufferTextureType::GBufferVelocity);

    if (!pColorIn || !pColorOut || !pDepth || !pMotion)
        return;

    const auto outputExtents = pColorOut->GetInfo().extents;

    // For now, just native mode (DLAA). Reconfigure only on mode/resolution changes.
    constexpr sl::DLSSMode kDlssMode = sl::DLSSMode::eDLAA;
    if (!Nvidia::StreamlineManager::EnsureDLSSConfigured(outputExtents.x, outputExtents.y, kDlssMode))
        return;
    if (Nvidia::StreamlineManager::IsDLSSEvaluateBlocked())
        return;

    // Transition layouts for DLSS
    // ColorOut should be in GENERAL for writing
    {
        ImageLayoutTransitionCmd colorOutGen(pColorOut);
        colorOutGen.oldLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
        colorOutGen.newLayout = ImageLayout::GENERAL;
        VkTextureManager::SetLayoutBarrierMasks(colorOutGen, ImageLayout::SHADER_READ_ONLY_OPTIMAL, ImageLayout::GENERAL);
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

    stltype::fixed_vector<StreamlineTagDesc, 4> tagDescs;
    auto pushTagDesc = [&](Texture* pTex, sl::BufferType type) {
        if (!pTex) return false;
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
        tagDescs.push_back(desc);
        return true;
    };

    const bool tagsOk = pushTagDesc(pColorIn, sl::kBufferTypeScalingInputColor) &&
                        pushTagDesc(pColorOut, sl::kBufferTypeScalingOutputColor) &&
                        pushTagDesc(pDepth, sl::kBufferTypeDepth) &&
                        pushTagDesc(pMotion, sl::kBufferTypeMotionVectors);
    if (!tagsOk)
    {
        restoreColorOutReadLayout();
        return;
    }

    // Get UBO data for matrices
    auto& sharedDataUBO = *reinterpret_cast<UBO::SharedDataUBO*>(ctx.pMappedSharedDataUBO);

    // Camera constants
    sl::Constants slConst{};
    constexpr f32 kDegToRad = 0.01745329251994329576923690768489f;
    slConst.cameraViewToClip = *(sl::float4x4*)&sharedDataUBO.projection;
    slConst.clipToCameraView = *(sl::float4x4*)&sharedDataUBO.projectionInverse;
    
    // Calculate clipToPrevClip: clip -> view -> world -> prevWorld -> prevView -> prevClip
    mathstl::Matrix clipToPrevClip = sharedDataUBO.projectionInverse * sharedDataUBO.viewInverse * sharedDataUBO.prevViewProjection;
    slConst.clipToPrevClip = *(sl::float4x4*)&clipToPrevClip;
    const mathstl::Matrix prevClipToClip = clipToPrevClip.Invert();
    slConst.prevClipToClip = *(sl::float4x4*)&prevClipToClip;

    const mathstl::Vector2 jitter =
        GenerateR2Jitter(static_cast<int>(FrameGlobals::GetJitterFrameNumber()));
    slConst.jitterOffset = {jitter.x, jitter.y};
    slConst.mvecScale = {1.0f, 1.0f};
    slConst.cameraPinholeOffset = {0.0f, 0.0f};
    slConst.cameraPos = {data.mainView.position.x, data.mainView.position.y, data.mainView.position.z};
    const mathstl::Vector3 rotation = mathstl::Vector3(data.mainView.rotation.x * kDegToRad,
                                                       data.mainView.rotation.y * kDegToRad,
                                                       data.mainView.rotation.z * kDegToRad);
    const mathstl::Matrix rotationMatrix = mathstl::Matrix::CreateFromYawPitchRoll(rotation.y, rotation.x, rotation.z);
    const mathstl::Vector3 cameraRight = mathstl::Vector3::Transform(mathstl::Vector3(1.0f, 0.0f, 0.0f), rotationMatrix);
    const mathstl::Vector3 cameraUp = mathstl::Vector3::Transform(mathstl::Vector3(0.0f, 1.0f, 0.0f), rotationMatrix);
    const mathstl::Vector3 cameraForwardBasis = mathstl::Vector3::Transform(mathstl::Vector3(0.0f, 0.0f, 1.0f), rotationMatrix);
    const mathstl::Vector3 cameraForward(-cameraForwardBasis.x, -cameraForwardBasis.y, -cameraForwardBasis.z);
    slConst.cameraUp = {cameraUp.x, cameraUp.y, cameraUp.z};
    slConst.cameraRight = {cameraRight.x, cameraRight.y, cameraRight.z};
    slConst.cameraFwd = {cameraForward.x, cameraForward.y, cameraForward.z};
    slConst.cameraNear = ctx.zNear;
    slConst.cameraFar = ctx.zFar;
    slConst.cameraFOV = data.mainView.fov * kDegToRad;
    slConst.cameraAspectRatio = (data.mainView.viewport.height > 0.0f)
                                    ? (data.mainView.viewport.width / data.mainView.viewport.height)
                                    : 1.0f;
    slConst.motionVectorsInvalidValue = 0.0f;
    // Depth prepass uses LESS_OR_EQUAL with depth clear=1.0, so depth is not inverted.
    slConst.depthInverted = sl::Boolean::eFalse;
    slConst.cameraMotionIncluded = sl::Boolean::eTrue;
    slConst.motionVectors3D = sl::Boolean::eFalse;
    slConst.reset = Nvidia::StreamlineManager::ConsumeDLSSResetFlag() ? sl::Boolean::eTrue : sl::Boolean::eFalse;
    slConst.motionVectorsJittered = sl::Boolean::eFalse;

    ExecuteNativeCmd streamlineCmd{};
    streamlineCmd.callback = [tagDescs, pFrameToken](void* pNativeCmdBuf) mutable {
        VkCommandBuffer cmd = reinterpret_cast<VkCommandBuffer>(pNativeCmdBuf);
        sl::ViewportHandle viewportHandle(0);
        if (!pFrameToken)
            return;

        stltype::fixed_vector<sl::Resource, 4> resources;
        stltype::fixed_vector<sl::ResourceTag, 4> tags;
        for (const auto& td : tagDescs)
        {
            sl::Resource& res = resources.emplace_back();
            res.type = sl::ResourceType::eTex2d;
            res.native = reinterpret_cast<void*>(td.native);
            res.view = reinterpret_cast<void*>(td.view);
            res.width = td.width;
            res.height = td.height;
            res.nativeFormat = td.nativeFormat;
            res.usage = td.usage;
            res.state = td.state;
            res.mipLevels = td.mipLevels;
            res.arrayLayers = td.arrayLayers;

            tags.emplace_back(&res, td.type, sl::ResourceLifecycle::eValidUntilEvaluate);
        }

        const sl::Result tagRes = slSetTagForFrame(*pFrameToken,
                                                   viewportHandle,
                                                   tags.data(),
                                                   static_cast<uint32_t>(tags.size()),
                                                   reinterpret_cast<sl::CommandBuffer*>(cmd));
        if (tagRes != sl::Result::eOk)
        {
            std::cout << "[DLSSPass] slSetTagForFrame failed with result: 0x" << std::hex
                      << static_cast<u32>(tagRes) << std::dec << std::endl;
            return;
        }

        Nvidia::StreamlineManager::EvaluateDLSS(cmd, *pFrameToken);
    };

    StartRenderPassProfilingScope(pCmdBuffer);
    const sl::Result constRes = slSetConstants(slConst, *pFrameToken, viewport);
    if (constRes != sl::Result::eOk)
    {
        std::cout << "[DLSSPass] slSetConstants failed with result: 0x" << std::hex
                  << static_cast<u32>(constRes) << std::dec << std::endl;
        EndRenderPassProfilingScope(pCmdBuffer);
        restoreColorOutReadLayout();
        return;
    }
    pCmdBuffer->RecordCommand(streamlineCmd);
    EndRenderPassProfilingScope(pCmdBuffer);

    // Transition ColorOut back to SHADER_READ for following passes
    restoreColorOutReadLayout();
}
