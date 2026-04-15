#include "DLSSPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/FrameGlobals.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Vulkan/VkCommandBuffer.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Core/Rendering/Core/Nvidia/StreamlineManager.h"
#include "Core/Rendering/Vulkan/VkTexture.h"
#include "Core/Rendering/Vulkan/VkBuffer.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"
#include "sl.h"
#include "sl_dlss.h"

using namespace RenderPasses;

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

    static mathstl::Vector2 s_lastExtents{0, 0};
    static bool s_resourcesAllocated = false;

    const auto extents = FrameGlobals::GetSwapChainExtent();
    VkCommandBuffer vkCmd = static_cast<CBufferVulkan*>(pCmdBuffer)->GetRef();
    u32 frameIdx = FrameGlobals::GetFrameNumber();
    
    sl::FrameToken* frameToken;
    slGetNewFrameToken(frameToken, &frameIdx);
    
    // For now, just native mode (DLAA)
    Nvidia::StreamlineManager::SetDLSSOptions(extents.x, extents.y, sl::DLSSMode::eDLAA);

    if (!s_resourcesAllocated || extents.x != s_lastExtents.x || extents.y != s_lastExtents.y)
    {
        Nvidia::StreamlineManager::AllocateResources(vkCmd, sl::kFeatureDLSS);
        s_lastExtents = extents;
        s_resourcesAllocated = true;
    }

    // Need: Color (In/Out), Depth, Motion Vectors
    Texture* pColorIn = data.pGbuffer->Get(GBufferTextureType::GBufferResolve); // Input color (TAA Resolve)
    Texture* pColorOut = data.pGbuffer->Get(GBufferTextureType::GBufferThisFrameColor); // Output
    Texture* pDepth = data.pMainDepthTexture; // Current Depth
    Texture* pMotion = data.pGbuffer->Get(GBufferTextureType::GBufferVelocity);

    // Transition layouts for DLSS
    // ColorOut should be in GENERAL for writing
    {
        ImageLayoutTransitionCmd colorOutGen(pColorOut);
        colorOutGen.oldLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
        colorOutGen.newLayout = ImageLayout::GENERAL;
        VkTextureManager::SetLayoutBarrierMasks(colorOutGen, ImageLayout::SHADER_READ_ONLY_OPTIMAL, ImageLayout::GENERAL);
        pCmdBuffer->RecordCommand(colorOutGen);
    }

    sl::ViewportHandle viewport(0);

    // Map textures to Streamline constants
    auto mapTexture = [&](Texture* pTex, sl::BufferType type, sl::ResourceLifecycle lifecycle = sl::ResourceLifecycle::eValidUntilEvaluate) {
        if (!pTex) return;
        TextureVulkan* pVkTex = static_cast<TextureVulkan*>(pTex);
        sl::Resource res{};
        res.type = sl::ResourceType::eTex2d;
        res.native = pVkTex->GetImage();
        res.view = pVkTex->GetImageView();
        res.width = pVkTex->GetInfo().extents.x;
        res.height = pVkTex->GetInfo().extents.y;
        res.nativeFormat = (uint32_t)pVkTex->GetInfo().format;
        res.usage = (uint32_t)pVkTex->GetInfo().usage;
        res.state = (uint32_t)(type == sl::kBufferTypeScalingOutputColor ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        
        sl::ResourceTag tag(&res, type, lifecycle);
        slSetTagForFrame(*frameToken, viewport, &tag, 1, vkCmd);
    };

    mapTexture(pColorIn, sl::kBufferTypeScalingInputColor);
    mapTexture(pColorOut, sl::kBufferTypeScalingOutputColor);
    mapTexture(pDepth, sl::kBufferTypeDepth);
    mapTexture(pMotion, sl::kBufferTypeMotionVectors);

    // Get UBO data for matrices
    auto& sharedDataUBO = *reinterpret_cast<UBO::SharedDataUBO*>(ctx.pMappedSharedDataUBO);

    // Camera constants
    sl::Constants slConst{};
    slConst.cameraViewToClip = *(sl::float4x4*)&sharedDataUBO.projection;
    slConst.clipToCameraView = *(sl::float4x4*)&sharedDataUBO.projectionInverse;
    
    // Calculate clipToPrevClip: clip -> view -> world -> prevWorld -> prevView -> prevClip
    mathstl::Matrix clipToPrevClip = sharedDataUBO.projectionInverse * sharedDataUBO.viewInverse * sharedDataUBO.prevViewProjection;
    slConst.clipToPrevClip = *(sl::float4x4*)&clipToPrevClip;

    slConst.jitterOffset = {0,0}; 
    slConst.cameraNear = ctx.zNear;
    slConst.cameraFar = ctx.zFar;
    slConst.depthInverted = sl::Boolean::eTrue; // Our renderer uses inverted depth (D32_SFLOAT, usually 1->0)
    
    slSetConstants(slConst, *frameToken, viewport);

    StartRenderPassProfilingScope(pCmdBuffer);
    Nvidia::StreamlineManager::EvaluateDLSS(vkCmd, frameIdx);
    EndRenderPassProfilingScope(pCmdBuffer);

    // Transition ColorOut back to SHADER_READ for following passes
    {
        ImageLayoutTransitionCmd colorOutRead(pColorOut);
        colorOutRead.oldLayout = ImageLayout::GENERAL;
        colorOutRead.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
        VkTextureManager::SetLayoutBarrierMasks(colorOutRead, ImageLayout::GENERAL, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        pCmdBuffer->RecordCommand(colorOutRead);
    }
}
