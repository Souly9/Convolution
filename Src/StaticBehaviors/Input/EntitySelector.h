#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Events/InputEvents.h"

namespace EntitySelector
{
	struct WorldPosMouseRay
	{
		DirectX::XMVECTOR worldOrigin{};
		DirectX::XMVECTOR invOrigin{};
		DirectX::XMVECTOR direction{};
		f32 distance{};
	};

	void RegisterCallbacks();
	void OnMouseMove(const MouseMoveEventData& data);
	void OnLeftMouseClick(const LeftMouseClickEventData& data);
	WorldPosMouseRay CreateRay(const DirectX::XMFLOAT4& deviceOrigin);

	void OnUpdate(const UpdateEventData& data);

	bool RayAABBIntersection(const DirectX::SimpleMath::Vector4& rayOrigin, const DirectX::SimpleMath::Vector3& invRayOrigin, f32 rayDist,
		const DirectX::SimpleMath::Vector3& aabbMin, const DirectX::SimpleMath::Vector3& aabbMax, f32& distance);

	DirectX::XMVECTOR VectorizedRayAABBIntersection(const DirectX::XMVECTOR& rayOriginX, const DirectX::XMVECTOR& rayOriginY, const DirectX::XMVECTOR& rayOriginZ,
		const DirectX::XMVECTOR& inRayDirX, const DirectX::XMVECTOR& inRayDirY, const DirectX::XMVECTOR& inRayDirZ, const DirectX::XMVECTOR& rayLength,
		const DirectX::XMVECTOR& aabMinX, const DirectX::XMVECTOR& aabMinY, const DirectX::XMVECTOR& aabMinZ,
		const DirectX::XMVECTOR& aabMaxX, const DirectX::XMVECTOR& aabMaxY, const DirectX::XMVECTOR& aabMaxZ);
}