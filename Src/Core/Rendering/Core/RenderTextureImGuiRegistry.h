#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/State/States.h"
#include "Core/Rendering/Core/ShadowMaps.h"

class RGResourceRegistry;


class RenderTextureImGuiRegistry
{
public:
    static constexpr const char* MATERIAL_TEXTURES_CATEGORY = "Material Textures";

    void ReleaseGBufferIdsForNextFrame();
    void ReleaseShadowMapIdsForNextFrame();
    void RegisterShadowMapTextures(const CascadedShadowMap& shadowMap);
    void RegisterGBufferTextures(RGResourceRegistry& registry);
    void RegisterRTTextures(const class RGResourceRegistry& registry);
    void RegisterMaterialTextures();
    void ReleaseMaterialTextures();
    // Per frame after RotateHistory: points the ping-pong viewer entries at this frame's half
    void PublishTextureViewerState(RGResourceRegistry& registry);

private:
    void PublishTextureViewerItems();

    stltype::vector<RendererState::TextureViewerItem> m_textureViewerItems{};
    stltype::vector<u64> m_csmCascadeImGuiIDs{};
    stltype::vector<u64> m_gbufferImGuiIDs{};
    stltype::vector<u64> m_rtImGuiIDs{};
    stltype::vector<u64> m_materialImGuiIDs{};
    Texture* m_pVelocityA{nullptr};
    Texture* m_pHistoryColorA{nullptr};
    u64 m_velocityIdA{0};
    u64 m_velocityIdB{0};
    u64 m_historyColorIdA{0};
    u64 m_historyColorIdB{0};
};
