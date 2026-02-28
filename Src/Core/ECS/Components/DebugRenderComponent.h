#pragma once
#include "Component.h"
#include "Core/Global/GlobalDefines.h"
#include "RenderComponent.h"

struct Mesh;
struct Material;

namespace ECS
{
namespace Components
{
struct DebugRenderComponent : public RenderComponent
{
    bool shouldRender{true};
};
} // namespace Components
} // namespace ECS