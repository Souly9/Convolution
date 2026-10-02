#include "CpuFrustumCulling.h"

#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "../../../../../Shaders/Globals/Scene.h"

namespace RenderingCore
{
void CpuFrustumCulling::Execute(SharedResourceManager& resourceManager,
                                const mathstl::Matrix& viewProj,
                                u32 viewIndex,
                                bool isCullingEnabled,
                                bool isCullingFrozen,
                                const stltype::vector<DirectX::XMFLOAT4X4>& transforms,
                                bool uploadSSBO)
{
    ScopedZone("CpuFrustumCulling::Execute");

    if (viewIndex >= kMaxViews)
    {
        return;
    }

    const u32 viewBit = (1u << viewIndex);
    const bool isInitialized = (m_initializedFrustumMask & viewBit) != 0;

    if (!isCullingFrozen || !isInitialized)
    {
        m_frozenFrustums[viewIndex].ExtractFromMatrix(viewProj);
        m_initializedFrustumMask |= viewBit;
    }

    auto& instanceData = resourceManager.GetInstanceData();
    if (instanceData.empty())
    {
        return;
    }

    const auto& masterVisibility = resourceManager.GetMasterInstanceVisibility();
    u32 culledCount = 0;
    const u32 totalCount = static_cast<u32>(instanceData.size());
    const Frustum& frustum = m_frozenFrustums[viewIndex];

    for (u32 i = 0; i < totalCount; ++i)
    {
        auto& instance = instanceData[i];
        const bool masterVisible = (i < masterVisibility.size()) ? (masterVisibility[i] != 0) : true;

        if (!masterVisible)
        {
            instance.SetViewVisible(viewIndex, false);
            culledCount++;
            continue;
        }

        if (!isCullingEnabled)
        {
            instance.SetViewVisible(viewIndex, true);
            continue;
        }

        const u32 transformIdx = static_cast<u32>(instance.aabbCenterTransIdx.w);
        const mathstl::Vector3 localCenter(instance.aabbCenterTransIdx.x, instance.aabbCenterTransIdx.y, instance.aabbCenterTransIdx.z);
        const mathstl::Vector3 localExtents(mathstl::abs(instance.aabbExtentsMatIdx.x),
                                            mathstl::abs(instance.aabbExtentsMatIdx.y),
                                            mathstl::abs(instance.aabbExtentsMatIdx.z));

        mathstl::Matrix worldMat = mathstl::Matrix::Identity;
        if (transformIdx < transforms.size())
        {
            worldMat = mathstl::Matrix(transforms[transformIdx]);
        }

        const mathstl::Vector3 worldCenter = mathstl::Vector3::Transform(localCenter, worldMat);

        const mathstl::Vector3 worldExtents(
            mathstl::abs(worldMat._11) * localExtents.x + mathstl::abs(worldMat._21) * localExtents.y + mathstl::abs(worldMat._31) * localExtents.z,
            mathstl::abs(worldMat._12) * localExtents.x + mathstl::abs(worldMat._22) * localExtents.y + mathstl::abs(worldMat._32) * localExtents.z,
            mathstl::abs(worldMat._13) * localExtents.x + mathstl::abs(worldMat._23) * localExtents.y + mathstl::abs(worldMat._33) * localExtents.z
        );

        const bool visible = frustum.IntersectsAABB(worldCenter, worldExtents);
        instance.SetViewVisible(viewIndex, visible);
        if (!visible)
        {
            culledCount++;
        }
    }

    if (viewIndex == 0)
    {
        g_engine.GetApplicationState().RegisterUpdateFunction([totalCount, culledCount](ApplicationState& state) {
            state.renderState.totalInstanceCount = totalCount;
            state.renderState.culledInstanceCount = culledCount;
        });
    }

    if (uploadSSBO)
    {
        resourceManager.UploadInstanceDataSSBO();
    }
}
} // namespace RenderingCore
