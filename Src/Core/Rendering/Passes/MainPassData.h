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
    struct PassManagerRenderState
    {
        bool recreatedThisFrame{false};
        mathstl::Vector2 renderResolution{};
        mathstl::Vector2 swapchainResolution{};
        mathstl::Vector2 jitter{};
        mathstl::Vector2 previousJitter{};
    };

    ::SharedResourceManager* pResourceManager{nullptr};
    PassManagerRenderState renderState{};
    RenderView mainView{};
    mathstl::Matrix mainCamViewMatrix{};
    mathstl::Matrix mainCamInvViewProj{};
    stltype::vector<CsmRenderView> csmViews;
    stltype::vector<RenderView> shadowViews;
    stltype::vector<DescriptorSet::Ptr> viewDescriptorSets;
    stltype::hash_map<UBO::DescriptorContentsType, DescriptorSet::Ptr> bufferDescriptors;
    CascadedShadowMap directionalLightShadowMap{};
    RT::RTSceneManager* pRTSceneManager{nullptr};
    u32 cascades{0};
    f32 csmStepSize{0.0f};
};
} // namespace RenderPasses
