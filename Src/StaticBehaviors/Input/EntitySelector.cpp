#include "EntitySelector.h"
#undef max
#undef min
#include "Core/Events/EventSystem.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/SceneGraph/Mesh.h"

mathstl::Matrix invProj{};
mathstl::Matrix invView{};
mathstl::Matrix viewProj{};
ECS::Entity mainCamEntity;

void EntitySelector::RegisterCallbacks()
{
    g_engine.GetEventSystem().AddMouseMoveEventCallback([](const auto& d) { OnMouseMove(d); });
    g_engine.GetEventSystem().AddLeftMouseClickEventCallback([](const auto& d) { OnLeftMouseClick(d); });
    g_engine.GetEventSystem().AddUpdateEventCallback(
        [](const UpdateEventData& d)
        {
            invProj = d.state.renderState.invMainCamProjectionMatrix;
            invView = d.state.renderState.invMainCamViewMatrix;
            viewProj = d.state.renderState.mainCamViewProjectionMatrix;
            mainCamEntity = d.state.mainCameraEntity;
        });
}

void EntitySelector::OnMouseMove(const MouseMoveEventData& data)
{
}

bool EntitySelector::RayAABBIntersection(const DirectX::SimpleMath::Vector3& rayOrigin,
                                         const DirectX::SimpleMath::Vector3& invRayDirection,
                                         f32 rayDist,
                                         const DirectX::SimpleMath::Vector3& aabbMin,
                                         const DirectX::SimpleMath::Vector3& aabbMax,
                                         f32& distance)
{
    f64 tmin = 0.0, tmax = INFINITY;
    f64 tx1 = (aabbMin.x - rayOrigin.x) * invRayDirection.x;
    f64 tx2 = (aabbMax.x - rayOrigin.x) * invRayDirection.x;

    tmin = stltype::max(tmin, stltype::min(tx1, tx2));
    tmax = stltype::min(tmax, stltype::max(tx1, tx2));

    f64 ty1 = (aabbMin.y - rayOrigin.y) * invRayDirection.y;
    f64 ty2 = (aabbMax.y - rayOrigin.y) * invRayDirection.y;

    tmin = stltype::max(tmin, stltype::min(ty1, ty2));
    tmax = stltype::min(tmax, stltype::max(ty1, ty2));

    f64 tz1 = (aabbMin.z - rayOrigin.z) * invRayDirection.z;
    f64 tz2 = (aabbMax.z - rayOrigin.z) * invRayDirection.z;

    tmin = stltype::max(tmin, stltype::min(tz1, tz2));
    tmax = stltype::min(tmax, stltype::max(tz1, tz2));

    distance = tmin;

    return tmax >= stltype::max(0.0, tmin) && tmin < rayDist;
}

DirectX::XMVECTOR EntitySelector::VectorizedRayAABBIntersection(const DirectX::XMVECTOR& rayOriginX,
                                                                const DirectX::XMVECTOR& rayOriginY,
                                                                const DirectX::XMVECTOR& rayOriginZ,
                                                                const DirectX::XMVECTOR& inRayDirX,
                                                                const DirectX::XMVECTOR& inRayDirY,
                                                                const DirectX::XMVECTOR& inRayDirZ,
                                                                const DirectX::XMVECTOR& rayLength,
                                                                const DirectX::XMVECTOR& aabMinX,
                                                                const DirectX::XMVECTOR& aabMinY,
                                                                const DirectX::XMVECTOR& aabMinZ,
                                                                const DirectX::XMVECTOR& aabMaxX,
                                                                const DirectX::XMVECTOR& aabMaxY,
                                                                const DirectX::XMVECTOR& aabMaxZ)
{
    using namespace DirectX;
    const auto tx1 = XMVectorMultiply(XMVectorSubtract(aabMinX, rayOriginX), inRayDirX);
    const auto tx2 = XMVectorMultiply(XMVectorSubtract(aabMaxX, rayOriginX), inRayDirX);

    auto tmin = XMVectorMin(tx1, tx2);
    auto tmax = XMVectorMax(tx1, tx2);

    const auto ty1 = XMVectorMultiply(XMVectorSubtract(aabMinY, rayOriginY), inRayDirY);
    const auto ty2 = XMVectorMultiply(XMVectorSubtract(aabMaxY, rayOriginY), inRayDirY);

    tmin = XMVectorMax(tmin, XMVectorMin(ty1, ty2));
    tmax = XMVectorMin(tmax, XMVectorMax(ty1, ty2));

    const auto tz1 = XMVectorMultiply(XMVectorSubtract(aabMinZ, rayOriginZ), inRayDirZ);
    const auto tz2 = XMVectorMultiply(XMVectorSubtract(aabMaxZ, rayOriginZ), inRayDirZ);

    tmin = XMVectorMax(tmin, XMVectorMin(tz1, tz2));
    tmax = XMVectorMin(tmax, XMVectorMax(tz1, tz2));

    const auto intersectionComps = XMVectorGreaterOrEqual(tmax, XMVectorMax(XMVectorZero(), tmin));
    const auto validIntersections =
        XMVectorAndInt(XMVectorEqual(intersectionComps, XMVectorGreater(rayLength, tmin)), g_XMOne);

    return XMVectorMultiply(validIntersections, tmin);
}

void EntitySelector::OnLeftMouseClick(const LeftMouseClickEventData& data)
{
    if (mainCamEntity.IsValid() == false)
        return;
    using namespace DirectX::SimpleMath;
    const Vector2 mousePos{(f32)data.mousePosX, (f32)data.mousePosY};
    const auto& resolution = g_renderer.GetSwapchainExtent();
    if (resolution.x <= 0.0f || resolution.y <= 0.0f)
        return;

    const float x = (2.0f * ((mousePos.x) / (resolution.x))) - 1.0f;
    const float y = 1.0f - (2.0f * ((mousePos.y) / (resolution.y)));
    const DirectX::XMFLOAT4 deviceCoords = {x, y, 0, 1};
    WorldPosMouseRay ray(CreateRay(deviceCoords));

    f32 overallMinDist = FLT_MAX;
    ECS::Entity rsltEntity;

    const auto& meshAABBs = g_engine.GetMeshManager().GetMeshAABBs();

    auto checkIntersections = [&](const auto& comps)
    {
        for (size_t i = 0; i < comps.size(); i++)
        {
            const auto entity = comps[i].entity;
            const auto* pTransform = g_engine.GetEntityManager().GetComponent<ECS::Components::Transform>(entity);
            if (!pTransform)
                continue;

            const auto* pMesh = comps[i].component.pMesh;
            if (!pMesh)
                continue;

            auto meshIt = meshAABBs.find(pMesh);
            if (meshIt == meshAABBs.end())
                continue;

            const auto& localAABB = meshIt->second;
            const Vector4 localCenter = localAABB.center;
            const Vector4 localExtents = localAABB.extents;

            const Vector3 aabbMin =
                Vector3(localCenter.x - localExtents.x, localCenter.y - localExtents.y, localCenter.z - localExtents.z);
            const Vector3 aabbMax =
                Vector3(localCenter.x + localExtents.x, localCenter.y + localExtents.y, localCenter.z + localExtents.z);

            Matrix invWorld = pTransform->worldModelMatrix.Invert();
            Vector3 localRayOrigin = Vector3::Transform(ray.worldOrigin, invWorld);
            Vector3 localRayDir = Vector3::TransformNormal(ray.direction, invWorld);
            localRayDir.Normalize();
            Vector3 localInvRayDir = Vector3(1.0f) / localRayDir;

            f32 localDist;
            if (RayAABBIntersection(localRayOrigin, localInvRayDir, ray.distance, aabbMin, aabbMax, localDist))
            {
                Vector3 localHitPoint = localRayOrigin + localRayDir * localDist;
                Vector3 worldHitPoint = Vector3::Transform(localHitPoint, pTransform->worldModelMatrix);
                f32 worldDist = Vector3::Distance(ray.worldOrigin, worldHitPoint);

                if (worldDist < overallMinDist)
                {
                    overallMinDist = worldDist;
                    rsltEntity = entity;
                }
            }
        }
    };

    checkIntersections(g_engine.GetEntityManager().GetComponentVector<ECS::Components::RenderComponent>());
    checkIntersections(g_engine.GetEntityManager().GetComponentVector<ECS::Components::DebugRenderComponent>());

    auto deslectEntity = [](const ECS::Entity& entity, bool select = false)
    {
        auto pRenderComp = g_engine.GetEntityManager().GetComponent<ECS::Components::RenderComponent>(entity);
        if (pRenderComp == nullptr)
            pRenderComp = (ECS::Components::RenderComponent*)
                              g_engine.GetEntityManager().GetComponent<ECS::Components::DebugRenderComponent>(entity);
        if (pRenderComp != nullptr)
            pRenderComp->isSelected = select;
    };
    if (rsltEntity.IsValid())
    {
        g_engine.GetApplicationState().RegisterUpdateFunction(
            [rsltEntity, deslectEntity](ApplicationState& state)
            {
                for (auto& selectedEntity : state.selectedEntities)
                {
                    deslectEntity(selectedEntity);
                }
                state.selectedEntities.clear();
                state.selectedEntities.push_back(rsltEntity);
                deslectEntity(rsltEntity, true);
                g_engine.GetEntityManager().MarkComponentDirty(rsltEntity, ECS::ComponentID<ECS::Components::Transform>::ID);
                g_engine.GetEntityManager().MarkComponentDirty(rsltEntity,
                                                     ECS::ComponentID<ECS::Components::RenderComponent>::ID);
            });
    }
    else
    {
        g_engine.GetApplicationState().RegisterUpdateFunction(
            [deslectEntity](ApplicationState& state)
            {
                for (auto& selectedEntity : state.selectedEntities)
                {
                    deslectEntity(selectedEntity);
                }
                state.selectedEntities.clear();
            });
    }
}

EntitySelector::WorldPosMouseRay EntitySelector::CreateRay(const mathstl::Vector4& deviceOrigin)
{
    auto deviceBegin = deviceOrigin;
    deviceBegin.z = 1.0f;
    deviceBegin.w = 1.f;
    mathstl::Vector4 tmpBegin = mathstl::Vector4::Transform(deviceBegin, invProj);

    auto deviceEnd = deviceOrigin;
    deviceEnd.z = 0.0f;
    deviceEnd.w = 1.f;
    mathstl::Vector4 tmpEnd = mathstl::Vector4::Transform(deviceEnd, invProj);

    mathstl::Vector4 worldCoordsOrigin = tmpBegin / tmpBegin.w;
    mathstl::Vector4 worldCoordsEnd = tmpEnd / tmpEnd.w;

    worldCoordsOrigin = mathstl::Vector4::Transform(worldCoordsOrigin, invView);
    worldCoordsEnd = mathstl::Vector4::Transform(worldCoordsEnd, invView);

    mathstl::Vector3 dir = (mathstl::Vector3)worldCoordsEnd - (mathstl::Vector3)worldCoordsOrigin;
    dir.Normalize();

    return {(mathstl::Vector3)worldCoordsOrigin, mathstl::Vector3(1.0f) / dir, dir, 10000.f};
}