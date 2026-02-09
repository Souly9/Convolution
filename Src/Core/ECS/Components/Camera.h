#pragma once
#include "Component.h"
#include "Core/Global/GlobalDefines.h"
#include "View.h"

namespace ECS
{
namespace Components
{
struct Camera : public IComponent
{
    float fov{45.f};
    float zNear{0.1f};
    float zFar{100.f};
    bool isMainCam{true};
};
} // namespace Components
} // namespace ECS