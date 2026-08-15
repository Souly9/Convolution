#pragma once

#include "Frustum.h"
#include "Core/Global/Typedefs.h"
#include <EASTL/array.h>

class SharedResourceManager;

namespace RenderingCore
{
class CpuFrustumCulling
{
public:
    static constexpr u32 kMaxViews = 16;

    CpuFrustumCulling() = default;

    void Execute(SharedResourceManager& resourceManager,
                 const mathstl::Matrix& viewProj,
                 u32 viewIndex,
                 bool isCullingEnabled,
                 bool isCullingFrozen,
                 const stltype::vector<DirectX::XMFLOAT4X4>& transforms,
                 bool uploadSSBO,
                 u32 frameIdx);

    const Frustum& GetFrozenFrustum(u32 viewIndex = 0) const
    {
        return m_frozenFrustums[viewIndex < kMaxViews ? viewIndex : 0];
    }

private:
    stltype::array<Frustum, kMaxViews> m_frozenFrustums{};
    u32 m_initializedFrustumMask{0};
};
} // namespace RenderingCore
