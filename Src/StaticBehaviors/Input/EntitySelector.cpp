#include "Core/Global/GlobalDefines.h"
#include "Core/SceneGraph/Mesh.h"
#include "EntitySelector.h"


DirectX::XMMATRIX invProj{};
DirectX::XMMATRIX invView{};
DirectX::XMMATRIX viewProj{};
ECS::Entity mainCamEntity;

void EntitySelector::RegisterCallbacks()
{
	g_pEventSystem->AddMouseMoveEventCallback([](const auto& d) { OnMouseMove(d); });
	g_pEventSystem->AddLeftMouseClickEventCallback([](const auto& d) { OnLeftMouseClick(d); });
	g_pEventSystem->AddUpdateEventCallback([](const UpdateEventData& d)
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

bool EntitySelector::RayAABBIntersection(const DirectX::SimpleMath::Vector4& rayOrigin, const DirectX::SimpleMath::Vector3& invRayDirection, f32 rayDist,
	const DirectX::SimpleMath::Vector3& aabbMin, const DirectX::SimpleMath::Vector3& aabbMax, f32& distance)
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

	// If tmax >= max(0.0, tmin), there is an intersection
	if (tmax >= stltype::max(0.0, tmin) && tmin < rayDist)
	{
		distance = tmin;  
		return true;      
	}

	return false;  // No intersection
}

DirectX::XMVECTOR EntitySelector::VectorizedRayAABBIntersection(const DirectX::XMVECTOR& rayOriginX, const DirectX::XMVECTOR& rayOriginY, const DirectX::XMVECTOR& rayOriginZ,
	const DirectX::XMVECTOR& inRayDirX, const DirectX::XMVECTOR& inRayDirY, const DirectX::XMVECTOR& inRayDirZ, const DirectX::XMVECTOR& rayLength,
	const DirectX::XMVECTOR& aabMinX, const DirectX::XMVECTOR& aabMinY, const DirectX::XMVECTOR& aabMinZ,
	const DirectX::XMVECTOR& aabMaxX, const DirectX::XMVECTOR& aabMaxY, const DirectX::XMVECTOR& aabMaxZ)
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
	const auto validIntersections = XMVectorAndInt(
		XMVectorEqual(intersectionComps, XMVectorGreater(rayLength, tmin)),
	g_XMOne);

	return XMVectorMultiply(validIntersections, tmin);
}

void EntitySelector::OnLeftMouseClick(const LeftMouseClickEventData& data)
{
	using namespace DirectX::SimpleMath;
	const Vector2 mousePos{(f32)data.mousePosX, (f32)data.mousePosY};
	const auto& resolution = FrameGlobals::GetSwapChainExtent();
	const float x = (2.0f * ((mousePos.x) / (resolution.x))) - 1.0f;
	const float y = 1.0f - (2.0f * ((mousePos.y) / (resolution.y)));
	const DirectX::XMFLOAT4 deviceCoords =
	{
		x,
		y,
		0,
		1
	};
	WorldPosMouseRay ray(CreateRay(deviceCoords));
	//ray.m_screenPos = mousePos;

	// Cast against scene
	auto renderComps = g_pEntityManager->GetComponentVector<ECS::Components::RenderComponent>();
	const auto& debugRenderComps = g_pEntityManager->GetComponentVector<ECS::Components::DebugRenderComponent>();

	for (const auto& debugRenderComp : debugRenderComps)
	{
		if (debugRenderComp.component.shouldRender)
		{
			renderComps.emplace_back((ECS::Components::RenderComponent)debugRenderComp.component, debugRenderComp.entity);
		}
	}
	f32 overallMinDist = FLT_MAX;
	ECS::Entity rsltEntity;
	if (renderComps.size() < 9)
	{
		const Vector3 rayDir(ray.direction);
		const Vector3 dirInverted = Vector3(1) / rayDir;

		for (size_t i = 0; i < renderComps.size(); i++)
		{
			const auto& aabb = renderComps[i].component.boundingBox;
			const Vector3 aabbCenter = aabb.center;
			const Vector3 aabbExtents = aabb.extents;
			const Vector3 aabbMin = Vector3(aabbCenter.x - aabbExtents.x, aabbCenter.y - aabbExtents.y, aabbCenter.z - aabbExtents.z);
			const Vector3 aabbMax = Vector3(aabbCenter.x + aabbExtents.x, aabbCenter.y + aabbExtents.y, aabbCenter.z + aabbExtents.z);

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
	}
	else
	{
		using namespace DirectX;
		XMVECTOR rayOriginX, rayOriginY, rayOriginZ;
		XMVECTOR invRayDirectionX, invRayDirectionY, invRayDirectionZ;
		XMVECTOR rayLength;
		{
			XMFLOAT3 rayWorldOrig;
			XMStoreFloat3(&rayWorldOrig, ray.worldOrigin);
			rayOriginX = XMVectorSet(rayWorldOrig.x, rayWorldOrig.x, rayWorldOrig.x, rayWorldOrig.x);
			rayOriginY = XMVectorSet(rayWorldOrig.y, rayWorldOrig.y, rayWorldOrig.y, rayWorldOrig.y);
			rayOriginZ = XMVectorSet(rayWorldOrig.z, rayWorldOrig.z, rayWorldOrig.z, rayWorldOrig.z);
		}
		{
			XMFLOAT3 rayDir;
			XMStoreFloat3(&rayDir, ray.direction);
			invRayDirectionX = XMVectorSet(1.0f / rayDir.x, 1.0f / rayDir.x, 1.0f / rayDir.x, 1.0f / rayDir.x);
			invRayDirectionY = XMVectorSet(1.0f / rayDir.y, 1.0f / rayDir.y, 1.0f / rayDir.y, 1.0f / rayDir.y);
			invRayDirectionZ = XMVectorSet(1.0f / rayDir.z, 1.0f / rayDir.z, 1.0f / rayDir.z, 1.0f / rayDir.z);
		}
		{
			rayLength = XMVectorSet(ray.distance, ray.distance, ray.distance, ray.distance);
		}
		auto FillAABBMinMaxVectors = [](DirectX::XMVECTOR& aabMinX, DirectX::XMVECTOR& aabMinY, DirectX::XMVECTOR& aabMinZ,
			DirectX::XMVECTOR& aabMaxX, DirectX::XMVECTOR& aabMaxY, DirectX::XMVECTOR& aabMaxZ,
			const AABB& aabb1, const AABB& aabb2, const AABB& aabb3, const AABB& aabb4)
			{
				const XMVECTOR aabbCentersX = XMVectorSet(aabb1.center.x, aabb2.center.x, aabb3.center.x, aabb4.center.x);
				const XMVECTOR aabbExtentsX = XMVectorSet(aabb1.extents.x, aabb2.extents.x, aabb3.extents.x, aabb4.extents.x);
				const XMVECTOR aabbCentersY = XMVectorSet(aabb1.center.y, aabb2.center.y, aabb3.center.y, aabb4.center.y);
				const XMVECTOR aabbExtentsY = XMVectorSet(aabb1.extents.y, aabb2.extents.y, aabb3.extents.y, aabb4.extents.y);
				const XMVECTOR aabbCentersZ = XMVectorSet(aabb1.center.z, aabb2.center.z, aabb3.center.z, aabb4.center.z);
				const XMVECTOR aabbExtentsZ = XMVectorSet(aabb1.extents.z, aabb2.extents.z, aabb3.extents.z, aabb4.extents.z);
				aabMinX = XMVectorSubtract(aabbCentersX, aabbExtentsX);
				aabMinY = XMVectorSubtract(aabbCentersY, aabbExtentsY);
				aabMinZ = XMVectorSubtract(aabbCentersZ, aabbExtentsZ);
				aabMaxX = XMVectorAdd(aabbCentersX, aabbExtentsX);
				aabMaxY = XMVectorAdd(aabbCentersY, aabbExtentsY);
				aabMaxZ = XMVectorAdd(aabbCentersZ, aabbExtentsZ);
			};

		for (size_t i = 0; i < renderComps.size(); i += 4)
		{
			const auto& aabb1 = renderComps[i].component.boundingBox;
			const auto& aabb2 = renderComps[i + 1].component.boundingBox;
			const auto& aabb3 = renderComps[i + 2].component.boundingBox;
			const auto& aabb4 = renderComps[i + 3].component.boundingBox;

			DirectX::XMVECTOR aabMinX, aabMinY, aabMinZ, aabMaxX, aabMaxY, aabMaxZ;
			FillAABBMinMaxVectors(aabMinX, aabMinY, aabMinZ, aabMaxX, aabMaxY, aabMaxZ, aabb1, aabb2, aabb3, aabb4);

			const XMVECTOR rayIntersectionResult = VectorizedRayAABBIntersection(rayOriginX, rayOriginY, rayOriginZ, 
				invRayDirectionX, invRayDirectionY, invRayDirectionZ, rayLength, 
				aabMinX, aabMinY, aabMinZ, 
				aabMaxX, aabMaxY, aabMaxZ);
			XMFLOAT4 rayDistances;
			XMStoreFloat4(&rayDistances, rayIntersectionResult);
			const f32 minDist = stltype::min(rayDistances.x, stltype::min(rayDistances.y, stltype::min(rayDistances.z, rayDistances.w)));

			if (minDist < overallMinDist)
			{
				overallMinDist = minDist;
				if ((minDist - rayDistances.x) <= FLOAT_TOLERANCE)
				{
					rsltEntity = renderComps[i].entity;
				}
				else if ((minDist - rayDistances.y) <= FLOAT_TOLERANCE)
				{
					rsltEntity = renderComps[i + 1].entity;
				}
				else if ((minDist - rayDistances.z) <= FLOAT_TOLERANCE)
				{
					rsltEntity = renderComps[i + 2].entity;
				}
				else if ((minDist - rayDistances.w) <= FLOAT_TOLERANCE)
				{
					rsltEntity = renderComps[i + 3].entity;
				}
			}
		}
	}

	if (rsltEntity.IsValid())
	{
		g_pApplicationState->RegisterUpdateFunction([rsltEntity](ApplicationState& state)
			{
				state.selectedEntities.clear();
				state.selectedEntities.push_back(rsltEntity);
			});
	}
	else
	{
		g_pApplicationState->RegisterUpdateFunction([rsltEntity](ApplicationState& state)
			{
				state.selectedEntities.clear();
			});
	}
}

EntitySelector::WorldPosMouseRay EntitySelector::CreateRay(const DirectX::XMFLOAT4& deviceOrigin)
{
	auto deviceBegin = deviceOrigin;
	deviceBegin.z = -1;
	deviceBegin.w = 1.f;
	mathstl::Vector4 tmpBegin = DirectX::XMVector4Transform(DirectX::XMLoadFloat4(&deviceBegin), invProj);

	auto deviceEnd = deviceOrigin;
	deviceEnd.z = 1.f;
	deviceEnd.w = 1.f;
	mathstl::Vector4 tmpEnd = DirectX::XMVector4Transform(DirectX::XMLoadFloat4(&deviceEnd), invProj);

	// Homogenize the transformed vectors by dividing by w
	auto worldCoordsOrigin = DirectX::XMVectorDivide(tmpBegin, DirectX::XMVectorSplatW(tmpBegin));
	auto worldCoordsEnd = DirectX::XMVectorDivide(tmpEnd, DirectX::XMVectorSplatW(tmpEnd));

	worldCoordsOrigin = DirectX::XMVector4Transform(worldCoordsOrigin, invView);
	worldCoordsEnd = DirectX::XMVector4Transform(worldCoordsEnd, invView);

	mathstl::Vector3 dir = DirectX::XMVectorSubtract(worldCoordsEnd, worldCoordsOrigin);
	dir.y *= -1.f;
	dir.Normalize();

	return { worldCoordsOrigin, dir, dir, 10000.f };
}