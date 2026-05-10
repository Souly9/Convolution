#pragma once
#include "Core/Rendering/Core/FrameResourceManager.h"
#include "Core/Rendering/Core/ShadowMaps.h"

class ShadowMapManager
{
public:
    void Recreate(u32 cascades, const mathstl::Vector2& extents, RenderPasses::FrameResourceManager& frameResourceManager);
    void Reset();

    const CascadedShadowMap& GetShadowMap() const { return m_shadowMap; }
    CascadedShadowMap& GetShadowMap() { return m_shadowMap; }

private:
    CascadedShadowMap m_shadowMap{};
};
