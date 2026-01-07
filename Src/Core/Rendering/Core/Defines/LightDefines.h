#pragma once
#include "Core/ECS/Components/Light.h"
#include "Core/ECS/Components/Transform.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/SceneGraph/Mesh.h"

struct RenderLight
{
    mathstl::Vector4 position{};
    mathstl::Vector4 direction{};
    mathstl::Vector4 color{1, 1, 0, 1};
    mathstl::Vector4 cutoff{};
};
// Mainly to differentiate between point lights and directional lights etc. for
// now, might merge these back but want to go with clarity for now
struct DirectionalRenderLight
{
    mathstl::Vector4 direction{};
    mathstl::Vector4 color{1, 1, 0, 1};
};

// Uniform data needed by fragment shaders for lighting
struct LightUniforms
{
    mathstl::Vector4 CameraPos;
    mathstl::Vector4 LightGlobals; // ambient
};

inline DirectionalRenderLight ConvertToDirectionalRenderLight(const ECS::Components::Light* light,
                                                              const ECS::Components::Transform* pTransform)
{
    DirectionalRenderLight renderLight;
    // Use the light's direction field, not the transform's position
    // w=0.0f indicates this is a direction vector, not a point
    renderLight.direction = mathstl::Vector4(light->direction.x, light->direction.y, light->direction.z, 0.0f);
    renderLight.color = light->color;

    return renderLight;
}

inline RenderLight ConvertToRenderLight(const ECS::Components::Light* light,
                                        const ECS::Components::Transform* transform)
{
    RenderLight renderLight;
    renderLight.position = mathstl::Vector4(transform->position.x, transform->position.y, transform->position.z, 1.0f);
    renderLight.direction = mathstl::Vector4(light->direction.x, light->direction.y, light->direction.z, 1.0f);
    renderLight.color = light->color;
    renderLight.cutoff = mathstl::Vector4(light->cutoff, 0, 0, 0);

    return renderLight;
}