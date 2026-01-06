#pragma once
#include "Core/Events/EventSystem.h"
#include "Core/Events/InputEvents.h"
#include "Core/Global/GlobalDefines.h"

namespace EntitySelector
{
struct WorldPosMouseRay
{
    mathstl::Vector3 worldOrigin{};
    mathstl::Vector3 invOrigin{};
    mathstl::Vector3 direction{};
    f32 distance{};
};

void RegisterCallbacks();
void OnMouseMove(const MouseMoveEventData& data);
void OnLeftMouseClick(const LeftMouseClickEventData& data);
WorldPosMouseRay CreateRay(const mathstl::Vector4& deviceOrigin);

void OnUpdate(const UpdateEventData& data);

bool RayAABBIntersection(const DirectX::SimpleMath::Vector3& rayOrigin,
                         const DirectX::SimpleMath::Vector3& invRayOrigin,
                         f32 rayDist,
                         const DirectX::SimpleMath::Vector3& aabbMin,
                         const DirectX::SimpleMath::Vector3& aabbMax,
                         f32& distance);

DirectX::XMVECTOR VectorizedRayAABBIntersection(const DirectX::XMVECTOR& rayOriginX,
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
                                                const DirectX::XMVECTOR& aabMaxZ);
} // namespace EntitySelector