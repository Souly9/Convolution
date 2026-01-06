#pragma once
#include "Component.h"
#include "Core/Global/GlobalDefines.h"

namespace ECS
{
namespace Components
{
struct Hierarchy : public IComponent
{
public:
    stltype::vector<ECS::Entity> children;
};
} // namespace Components
} // namespace ECS