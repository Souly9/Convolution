#include "EntitySelector.h"
#undef max
#undef min
#include "Core/Events/EventSystem.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/SceneGraph/Mesh.h"
#include "StaticBehaviors/Input/SelectedEntityMover.h"

mathstl::Matrix invProj{};
mathstl::Matrix invView{};
mathstl::Matrix viewProj{};
ECS::Entity mainCamEntity;

void EntitySelector::RegisterCallbacks()
{
    g_pEventSystem->AddMouseMoveEventCallback([](const auto& d) { OnMouseMove(d); });
    g_pEventSystem->AddLeftMouseClickEventCallback([](const auto& d) { OnLeftMouseClick(d); });
    g_pEventSystem->AddUpdateEventCallback(
        [](const UpdateEventData& d)
        {
            invProj = d.state.invMainCamProjectionMatrix;
            invView = d.state.invMainCamViewMatrix;
            viewProj = d.state.mainCamViewProjectionMatrix;
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
    using namespace DirectX::SimpleMath;
    const Vector2 mousePos{(f32)data.mousePosX, (f32)data.mousePosY};
    const auto& resolution = FrameGlobals::GetSwapChainExtent();
    const float x = (2.0f * ((mousePos.x) / (resolution.x))) - 1.0f;
    const float y = 1.0f - (2.0f * ((mousePos.y) / (resolution.y)));
    const DirectX::XMFLOAT4 deviceCoords = {x, y, 0, 1};
    WorldPosMouseRay ray(CreateRay(deviceCoords));
    // ray.m_screenPos = mousePos;

    // Cast against scene
    auto& renderComps = g_pEntityManager->GetComponentVector<ECS::Components::RenderComponent>();
    const auto& debugRenderComps = g_pEntityManager->GetComponentVector<ECS::Components::DebugRenderComponent>();

    for (const auto& debugRenderComp : debugRenderComps)
    {
        if (debugRenderComp.component.shouldRender)
        {
            renderComps.emplace_back((ECS::Components::RenderComponent)debugRenderComp.component,
                                     debugRenderComp.entity);
        }
    }
    f32 overallMinDist = FLT_MAX;
    ECS::Entity rsltEntity;
    const Vector3 rayDir(ray.direction);
    const Vector3 dirInverted = ray.invOrigin;

    for (size_t i = 0; i < renderComps.size(); i++)
    {
        const auto& aabb = renderComps[i].component.boundingBox;
        const Vector3 aabbCenter = aabb.center;
        const Vector3 aabbExtents = aabb.extents;
        const Vector3 aabbMin =
            Vector3(aabbCenter.x - aabbExtents.x, aabbCenter.y - aabbExtents.y, aabbCenter.z - aabbExtents.z);
        const Vector3 aabbMax =
            Vector3(aabbCenter.x + aabbExtents.x, aabbCenter.y + aabbExtents.y, aabbCenter.z + aabbExtents.z);

        f32 dist;
        if (RayAABBIntersection(ray.worldOrigin, dirInverted, ray.distance, aabbMin, aabbMax, dist))
        {
            if (dist < overallMinDist)
            {
                overallMinDist = dist;
                rsltEntity = renderComps[i].entity;
            }
        }
    }

    auto deslectEntity = [](const ECS::Entity& entity, bool select = false)
    {
        auto pRenderComp = g_pEntityManager->GetComponent<ECS::Components::RenderComponent>(entity);
        if (pRenderComp == nullptr)
            pRenderComp = (ECS::Components::RenderComponent*)
                              g_pEntityManager->GetComponent<ECS::Components::DebugRenderComponent>(entity);
        if (pRenderComp != nullptr)
            pRenderComp->isSelected = select;
    };
    if (rsltEntity.IsValid())
    {
        g_pApplicationState->RegisterUpdateFunction(
            [rsltEntity, deslectEntity](ApplicationState& state)
            {
                // Clear previous selection
                for (auto& selectedEntity : state.selectedEntities)
                {
                    deslectEntity(selectedEntity);
                }
                state.selectedEntities.clear();
                state.selectedEntities.push_back(rsltEntity);
                deslectEntity(rsltEntity, true);
                g_pEntityManager->MarkComponentDirty(rsltEntity, ECS::ComponentID<ECS::Components::Transform>::ID);
                g_pEntityManager->MarkComponentDirty(rsltEntity,
                                                     ECS::ComponentID<ECS::Components::RenderComponent>::ID);
            });
    }
    else
    {
        g_pApplicationState->RegisterUpdateFunction(
            [rsltEntity, deslectEntity](ApplicationState& state)
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
    deviceBegin.z = -1;
    deviceBegin.w = 1.f;
    mathstl::Vector4 tmpBegin = mathstl::Vector4::Transform(deviceBegin, invProj);

    auto deviceEnd = deviceOrigin;
    deviceEnd.z = 1.f;
    deviceEnd.w = 1.f;
    mathstl::Vector4 tmpEnd = mathstl::Vector4::Transform(deviceEnd, invProj);

    // Homogenize the transformed vectors by dividing by w
    mathstl::Vector4 worldCoordsOrigin = tmpBegin / tmpBegin.w;
    mathstl::Vector4 worldCoordsEnd = tmpEnd / tmpEnd.w;

    worldCoordsOrigin = mathstl::Vector4::Transform(worldCoordsOrigin, invView);
    worldCoordsEnd = mathstl::Vector4::Transform(worldCoordsEnd, invView);

    mathstl::Vector3 dir = (mathstl::Vector3)worldCoordsEnd - (mathstl::Vector3)worldCoordsOrigin;
    dir.y *= -1.f;
    dir.Normalize();

    return {(mathstl::Vector3)worldCoordsOrigin, mathstl::Vector3(1) / dir, dir, 10000.f};
}