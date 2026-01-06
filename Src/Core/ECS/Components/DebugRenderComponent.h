#pragma once
#include "Component.h"
#include "Core/Global/GlobalDefines.h"
#include "RenderComponent.h"

class Mesh;
class Material;

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