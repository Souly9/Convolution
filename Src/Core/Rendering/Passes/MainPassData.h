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
struct SharedDataUBO;
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
    };

    ::SharedResourceManager* pResourceManager{nullptr};
    PassManagerRenderState renderState{};
    RenderView mainView{};
    // CPU copy of the view UBO for this frame
    const ::SharedDataUBO* pViewData{nullptr};
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
