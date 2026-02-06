#pragma once
#include "Component.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/AABB.h"

class Mesh;
class Material;

namespace ECS
{
namespace Components
{
struct RenderComponent : public IComponent
{
    Mesh* pMesh;
    Material* pMaterial{nullptr};

    AABB boundingBox{};
    bool isSelected{false};
    bool isWireframe{false};
};
} // namespace Components
} // namespace ECS