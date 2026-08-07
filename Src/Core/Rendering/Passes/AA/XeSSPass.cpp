#include "XeSSPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Core/Rendering/Vulkan/XeSS/XeSSManager.h"
#include "Core/Rendering/Vulkan/VkTexture.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "Core/Rendering/Vulkan/Utils/VkEnumHelpers.h"
#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"
#include <cstring>

using namespace RenderPasses;

namespace
{
xess_quality_settings_t ResolveXeSSQualityModeForRenderScale(const mathstl::Vector2& renderResolution,
                                                             const mathstl::Vector2& swapchainResolution)
{
    if (swapchainResolution.x <= 0.0f || renderResolution.x >= swapchainResolution.x)
        return XESS_QUALITY_SETTING_AA;

    const f32 upscalingPercentage = (renderResolution.x * 100.0f) / swapchainResolution.x;
    if (upscalingPercentage >= 90)
        return XESS_QUALITY_SETTING_ULTRA_QUALITY_PLUS;
    if (upscalingPercentage >= 80)
        return XESS_QUALITY_SETTING_ULTRA_QUALITY;
    if (upscalingPercentage >= 70)
        return XESS_QUALITY_SETTING_QUALITY;
    if (upscalingPercentage >= 60)
        return XESS_QUALITY_SETTING_BALANCED;
    if (upscalingPercentage >= 50)
        return XESS_QUALITY_SETTING_PERFORMANCE;
    return XESS_QUALITY_SETTING_ULTRA_PERFORMANCE;
}
}

XeSSPass::XeSSPass() : ConvolutionRenderPass("XeSSPass")
{
}

XeSSPass::~XeSSPass()
{
}

void XeSSPass::Init(const SharedResourceManager& resourceManager)
{
}

bool XeSSPass::WantsToRender() const
{
    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    const bool wantsToRender = VulkanXeSS::XeSSManager::IsSupported() && renderState.aaType == AntialiasingType::XeSS;
    if (!wantsToRender)
        m_wasActive = false;
    return wantsToRender;
}

void XeSSPass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("XeSSPass::Render");

    Texture* pColorIn = execCtx.GetTexture(RGResourceID::GBufferThisFrameColor);
    Texture* pColorOut = execCtx.GetTexture(RGResourceID::TemporalResolve);
    Texture* pDepth = execCtx.GetTexture(RGResourceID::MainDepth);
    Texture* pMotion = execCtx.GetTexture(RGResourceID::GBufferVelocity);

    if (!pColorIn || !pColorOut || !pDepth || !pMotion)
        return;

    const auto outputExtents = pColorOut->GetInfo().extents;
    const auto inputExtents = pColorIn->GetInfo().extents;

    const xess_quality_settings_t xessQuality =
        ResolveXeSSQualityModeForRenderScale(data.renderState.renderResolution, data.renderState.swapchainResolution);

    const auto& context = VkGlobals::GetContext();
    if (!VulkanXeSS::XeSSManager::EnsureConfigured(context.Instance,
                                                   context.PhysicalDevice,
                                                   context.Device,
                                                   outputExtents.x,
                                                   outputExtents.y,
                                                   xessQuality))
        return;

    // Transition layouts for XeSS
    // ColorOut should be in GENERAL for writing
    auto to_xess_image_view = [](Texture* texture) -> xess_vk_image_view_info {
        TextureVulkan* pVkTex = static_cast<TextureVulkan*>(texture);
        xess_vk_image_view_info info = {};
        info.image = pVkTex->GetImage();
        info.imageView = pVkTex->GetImageView();

        const TexFormat format = pVkTex->GetInfo().format;
        const bool isDepthFormat = (format == TexFormat::D16_UNORM || format == TexFormat::X8_D24_UNORM_PACK32 ||
                                    format == TexFormat::D32_SFLOAT || format == TexFormat::D16_UNORM_S8_UINT ||
                                    format == TexFormat::D24_UNORM_S8_UINT || format == TexFormat::D32_SFLOAT_S8_UINT);
        const bool hasStencil = (format == TexFormat::D16_UNORM_S8_UINT || format == TexFormat::D24_UNORM_S8_UINT ||
                                 format == TexFormat::D32_SFLOAT_S8_UINT);

        info.subresourceRange.aspectMask = isDepthFormat ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        if (isDepthFormat && hasStencil)
            info.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;
        info.format = Conv(format);
        info.width = pVkTex->GetInfo().extents.x;
        info.height = pVkTex->GetInfo().extents.y;
        return info;
    };

    xess_vk_execute_params_t execParams{};
    execParams.colorTexture = to_xess_image_view(pColorIn);
    execParams.velocityTexture = to_xess_image_view(pMotion);
    execParams.depthTexture = to_xess_image_view(pDepth);
    execParams.outputTexture = to_xess_image_view(pColorOut);
    execParams.exposureScaleTexture = {}; 
    execParams.responsivePixelMaskTexture = {};

    const auto& cameraData = ctx.cameraData;
    const mathstl::Vector2 streamlineJitter = cameraData.jitterOffset;
    execParams.jitterOffsetX = streamlineJitter.x;
    execParams.jitterOffsetY = streamlineJitter.y;
    execParams.exposureScale = 1.0f;
    execParams.inputWidth = inputExtents.x;
    execParams.inputHeight = inputExtents.y;
    execParams.inputColorBase = {0, 0};
    execParams.inputMotionVectorBase = {0, 0};
    execParams.inputDepthBase = {0, 0};
    execParams.inputResponsiveMaskBase = {0, 0};
    execParams.outputColorBase = {0, 0};

    const bool shouldReset = !m_wasActive || data.renderState.recreatedThisFrame || VulkanXeSS::XeSSManager::ConsumeResetFlag();
    execParams.resetHistory = shouldReset ? 1u : 0u;

    ExecuteNativeCmd xessCmd{};
    xessCmd.callback = [execParams](void* pNativeCmdBuf) {
        VkCommandBuffer cmd = reinterpret_cast<VkCommandBuffer>(pNativeCmdBuf);
        VulkanXeSS::XeSSManager::Execute(cmd, execParams);
    };

    StartRenderPassProfilingScope(execCtx.pCmdBuffer);
    m_wasActive = true;
    execCtx.pCmdBuffer->RecordCommand(xessCmd);
    EndRenderPassProfilingScope(execCtx.pCmdBuffer);
}

void XeSSPass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.ReadTexture(RGResourceID::GBufferThisFrameColor, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::MainDepth, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::GBufferVelocity, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    auto resolve = builder.DeclareStorageTexture(RGResourceID::TemporalResolve, TexFormat::R16G16B16A16_FLOAT, RGSizeClass::OutputResolution);
    builder.WriteStorageImage(resolve, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_WRITE);
    builder.SetHasSideEffects();
}
