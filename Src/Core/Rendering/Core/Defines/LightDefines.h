#pragma once
#include "Core/ECS/Components/Light.h"
#include "Core/ECS/Components/Transform.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/SceneGraph/Mesh.h"

// GPU-compatible light structure (64 bytes, cache-aligned)
// position.w = type, direction.w = range, color.w = intensity, cutoff = spot params
struct RenderLight
{
    mathstl::Vector4 position{};        // xyz=position, w=type (LightType as float)
    mathstl::Vector4 direction{};       // xyz=direction, w=range
    mathstl::Vector4 color{1, 1, 0, 1}; // xyz=color, w=intensity
    mathstl::Vector4 cutoff{};          // x=inner cone, y=outer cone, zw=reserved

    f32 GetRange() const
    {
        return direction.w;
    }
    void SetRange(f32 r)
    {
        direction.w = r;
    }
    ECS::Components::LightType GetType() const
    {
        return static_cast<ECS::Components::LightType>(static_cast<u32>(position.w));
    }
    void SetType(ECS::Components::LightType t)
    {
        position.w = static_cast<f32>(static_cast<u32>(t));
    }
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
    mathstl::Vector4 LightGlobals;  // x = exposure, y = toneMapperType, z = ambientIntensity, w = csmDebugView
    mathstl::Vector4 ClusterValues; // x=scale, y=bias, z=maxSlices, w=shadowsEnabled
    mathstl::Vector4 ClusterSize;   // x=dimX, y=dimY, z=dimZ, w=tileSizeX
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
    renderLight.position = mathstl::Vector4(transform->position.x, transform->position.y, transform->position.z, 0.0f);
    renderLight.direction = mathstl::Vector4(light->direction.x, light->direction.y, light->direction.z, light->range);
    renderLight.color = mathstl::Vector4(light->color.x, light->color.y, light->color.z, light->intensity);
    renderLight.cutoff = mathstl::Vector4(light->cutoff, light->outerCutoff, 0, 0);

    // Set light type
    switch (light->type)
    {
        case ECS::Components::LightType::Directional:
            renderLight.SetType(ECS::Components::LightType::Directional);
            break;
        case ECS::Components::LightType::Spot:
            renderLight.SetType(ECS::Components::LightType::Spot);
            break;
        case ECS::Components::LightType::Point:
        default:
            renderLight.SetType(ECS::Components::LightType::Point);
            break;
    }

    return renderLight;
}