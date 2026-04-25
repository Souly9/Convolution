#pragma once
#include "Core/ECS/Components/Light.h"
#include "Core/ECS/Components/Transform.h"
#include "../../../../Shaders/Globals/ClusterLightData.h"

using RenderLight = Light;
using DirectionalRenderLight = DirectionalLight;

inline DirectionalRenderLight ConvertToDirectionalRenderLight(const ECS::Components::Light* light,
                                                              const ECS::Components::Transform* pTransform)
{
    (void)pTransform;

    DirectionalRenderLight renderLight;
    renderLight.direction = mathstl::Vector4(light->direction.x, light->direction.y, light->direction.z, 0.0f);
    renderLight.color = mathstl::Vector4(light->color.x, light->color.y, light->color.z, light->intensity);
    return renderLight;
}

inline RenderLight ConvertToRenderLight(const ECS::Components::Light* light,
                                        const ECS::Components::Transform* transform)
{
    RenderLight renderLight;
    renderLight.position = mathstl::Vector4(transform->position.x, transform->position.y, transform->position.z, 0.0f);
    renderLight.direction = mathstl::Vector4(light->direction.x, light->direction.y, light->direction.z, light->range);
    renderLight.color = mathstl::Vector4(light->color.x, light->color.y, light->color.z, light->intensity);
    renderLight.cutoff = mathstl::Vector4(light->cutoff, light->outerCutoff, 0.0f, 0.0f);

    renderLight.position.w = static_cast<f32>(static_cast<u32>(light->type));
    return renderLight;
}
