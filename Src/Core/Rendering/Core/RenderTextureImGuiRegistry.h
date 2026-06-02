#pragma once
#include "Core/Rendering/Core/GBuffer.h"
#include "Core/Rendering/Core/ShadowMaps.h"

namespace RT
{
class RTResourceManager;
}

class RenderTextureImGuiRegistry
{
public:
    void ReleaseGBufferIdsForNextFrame();
    void ReleaseShadowMapIdsForNextFrame();
    void RegisterShadowMapTextures(const CascadedShadowMap& shadowMap);
    void RegisterGBufferTextures(GBuffer& gbuffer, Texture* pScreenSpaceShadowTexture);
    void RegisterRTTextures(const RT::RTResourceManager& rtResourceManager);
    void PublishGBufferTextureState(GBuffer& gbuffer);

private:
    stltype::vector<u64> m_csmCascadeImGuiIDs{};
    stltype::vector<u64> m_gbufferImGuiIDs{};
    stltype::vector<u64> m_rtImGuiIDs{};
    Texture* m_pVelocityA{nullptr};
    Texture* m_pHistoryColorA{nullptr};
    u64 m_velocityIdA{0};
    u64 m_velocityIdB{0};
    u64 m_historyColorIdA{0};
    u64 m_historyColorIdB{0};
};
