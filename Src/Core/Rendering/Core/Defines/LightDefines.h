#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/ECS/Components/Light.h"
#include "Core/SceneGraph/Mesh.h"

static inline constexpr MeshManager::PrimitiveType s_lightDebugMeshPrimitive = MeshManager::PrimitiveType::Cube;

struct RenderLight
{
	mathstl::Vector4 position{};
	mathstl::Vector4 direction{};
	mathstl::Vector4 color{1,1,0,1};
	mathstl::Vector4 cutoff{};
};

// Uniform data needed by fragment shaders for lighting
struct LightUniforms
{
	mathstl::Vector4 CameraPos;
	mathstl::Vector4 LightGlobals; // ambient
};

inline RenderLight ConvertToRenderLight(const ECS::Components::Light* light, const ECS::Components::Transform* transform)
{
	RenderLight renderLight;
	renderLight.position = mathstl::Vector4(transform->position.x, transform->position.y, transform->position.z, 1.0f);
	renderLight.direction = mathstl::Vector4(light->direction.x, light->direction.y, light->direction.z, 1.0f);
	renderLight.color = light->color;
	renderLight.cutoff = mathstl::Vector4(light->cutoff, 0,0,0);

	return renderLight;
}