#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/Components/Light.h"
#include "Core/SceneGraph/Mesh.h"

static inline constexpr MeshManager::PrimitiveType s_lightDebugMeshPrimitive = MeshManager::PrimitiveType::Cube;

struct RenderLight
{
	DirectX::XMFLOAT4 position{};
	DirectX::XMFLOAT4 direction{};

	DirectX::XMFLOAT4 color{};

	DirectX::XMFLOAT4 cutoff{};
};

inline RenderLight ConvertToRenderLight(const ECS::Components::Light* light, const ECS::Components::Transform* transform)
{
	RenderLight renderLight;
	renderLight.position = DirectX::XMFLOAT4(transform->position.x, transform->position.y, transform->position.z, 1.0f);
	renderLight.direction = DirectX::XMFLOAT4(light->direction.x, light->direction.y, light->direction.z, 1.0f);
	renderLight.color = light->color;
	renderLight.cutoff = DirectX::XMFLOAT4(light->cutoff, 0,0,0);

	return renderLight;
}