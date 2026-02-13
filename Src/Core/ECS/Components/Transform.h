#pragma once
#include "Component.h"
#include "Core/ECS/Entity.h"
#include "Core/Global/GlobalDefines.h"
#include <DirectXMath.h>

namespace ECS
{
namespace Components
{
struct Transform : public IComponent
{
public:
    mathstl::Matrix localModelMatrix;
    mathstl::Matrix worldModelMatrix;

    mathstl::Vector3 position{0.0f, 0.0f, 0.0f};
    mathstl::Vector3 rotation{0.0f, 0.0f, 0.0f};
    mathstl::Vector3 scale{1.0f, 1.0f, 1.0f};

    mathstl::Vector3 worldPosition{0.0f, 0.0f, 0.0f};
    mathstl::Quaternion worldRotation{0.0f, 0.0f, 0.0f, 1.0f};
    mathstl::Vector3 worldScale{1.0f, 1.0f, 1.0f};

    stltype::string name;

    ECS::Entity parent;
    stltype::vector<ECS::Entity> children;

    Transform(const mathstl::Vector3& pos) : position{pos}
    {
    }

    void SetName(const stltype::string& n)
    {
        name = n;
    }

    bool HasParent() const
    {
        return parent.ID != INVALID_ENTITY;
    }

    void Scale(f32 s);
    void Scale(const mathstl::Vector3& s);
};
} // namespace Components
} // namespace ECS