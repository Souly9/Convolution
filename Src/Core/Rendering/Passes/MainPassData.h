#pragma once
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/Defines/UBODefines.h"
#include "Core/Rendering/Core/DescriptorSetLayout.h"
#include "Core/Rendering/Core/GBuffer.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/ShadowMaps.h"
#include "Core/Rendering/Core/View.h"
#include "../../../../Shaders/Globals/Types.h"

class SharedResourceManager;
namespace RT
{
class RTSceneManager;
}

namespace RenderPasses
{
struct MainPassData
{
    struct TemporalResources
    {
        Texture* pCurrentColorTexture{nullptr};
        Texture* pHistoryColorTexture{nullptr};
        Texture* pResolveTexture{nullptr};
        Texture* pPostAAColorTexture{nullptr};
        Texture* pCurrentDepthTexture{nullptr};
        Texture* pHistoryDepthTexture{nullptr};
        BindlessTextureHandle currentColorHandle{0};
        BindlessTextureHandle historyColorHandle{0};
        BindlessTextureHandle resolveHandle{0};
        BindlessTextureHandle postAAColorHandle{0};
        BindlessTextureHandle currentDepthHandle{0};
        BindlessTextureHandle historyDepthHandle{0};
    };

    struct PassManagerRenderState
    {
        bool recreatedThisFrame{false};
        mathstl::Vector2 renderResolution{};
        mathstl::Vector2 swapchainResolution{};
        mathstl::Vector2 jitter{};
        mathstl::Vector2 previousJitter{};
    };

    ::SharedResourceManager* pResourceManager{nullptr};
    GBuffer* pGbuffer{nullptr};
    Texture* pMainDepthTexture{nullptr};
    Texture* pLastFrameDepthTexture{nullptr};
    TemporalResources temporalResources{};
    PassManagerRenderState renderState{};
    RenderView mainView{};
    mathstl::Matrix mainCamViewMatrix{};
    mathstl::Matrix mainCamInvViewProj{};
    stltype::vector<CsmRenderView> csmViews;
    stltype::vector<RenderView> shadowViews;
    stltype::vector<DescriptorSet::Ptr> viewDescriptorSets;
    stltype::hash_map<UBO::DescriptorContentsType, DescriptorSet::Ptr> bufferDescriptors;
    CascadedShadowMap directionalLightShadowMap{};
    Texture* pScreenSpaceShadowTexture{nullptr};
    BindlessTextureHandle screenSpaceShadows{0};
    BindlessTextureHandle depthBufferBindlessHandle{0};
    RT::RTSceneManager* pRTSceneManager{nullptr};
    Texture* pRTDebugViewTexture{nullptr};
    Texture* pRTReflectionsTexture{nullptr};
    BindlessTextureHandle rtDebugTextureHandle{0};
    BindlessTextureHandle rtReflectionsTextureHandle{0};
    Texture* pRTAOTexture{nullptr};
    BindlessTextureHandle rtaoTextureHandle{0};
    Texture* pRTAccumulationTexture{nullptr};
    BindlessTextureHandle rtAccumulationTextureHandle{0};
    u32 cascades{0};
    f32 csmStepSize{0.0f};

    Texture* pSMAAEdgesTexture{nullptr};
    Texture* pSMAABlendTexture{nullptr};
    BindlessTextureHandle smaaEdges{0};
    BindlessTextureHandle smaaBlend{0};
};
} // namespace RenderPasses
