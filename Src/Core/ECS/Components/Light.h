#pragma once
#include "Component.h"
#include "Core/Global/GlobalDefines.h"
#include "SimpleMath/SimpleMath.h"

namespace ECS
{
namespace Components
{
enum class LightType
{
    Directional,
    Spot,
    Point
};
struct Light : public IComponent
{
public:
    mathstl::Vector3 direction{0.0f, 0.0f, 0.0f};
    mathstl::Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};

    f32 range{10.0f};       // Light falloff range for point/spot lights
    f32 intensity{1.0f};    // Light intensity multiplier
    f32 cutoff{45.0f};      // Inner cone angle for spot lights (degrees)
    f32 outerCutoff{60.0f}; // Outer cone angle for spot lights (degrees)
    LightType type{LightType::Point};
    bool isShadowCaster{false};
};
} // namespace Components
} // namespace ECS