#pragma once
#include "Components/Camera.h"
#include "Components/DebugRenderComponent.h"
#include "Components/Light.h"
#include "Components/RenderComponent.h"
#include "Components/Transform.h"
#include "Components/View.h"

struct Transform;
struct RenderComponent;
struct View;

namespace ECS::Components
{
struct Light;
}

using C_ID = u64;

namespace ECS
{
static inline constexpr u32 MAX_COMPONENTS = 32;

template <typename T>
struct ComponentID
{
    static constexpr C_ID ID = 0;
};
template <>
struct ComponentID<Components::Transform>
{
    static constexpr C_ID ID = 0x00000001;
};
template <>
struct ComponentID<Components::RenderComponent>
{
    static constexpr C_ID ID = 0x00000002;
};
template <>
struct ComponentID<Components::View>
{
    static constexpr C_ID ID = 0x00000004;
};
template <>
struct ComponentID<Components::Camera>
{
    static constexpr C_ID ID = 0x00000008;
};
template <>
struct ComponentID<Components::Light>
{
    static constexpr C_ID ID = 0x00000000F;
};
template <>
struct ComponentID<Components::DebugRenderComponent>
{
    static constexpr C_ID ID = 0x000000010;
};

static inline stltype::vector<C_ID> GetAllComponentIDs()
{
    return {ComponentID<Components::Transform>::ID,
            ComponentID<Components::RenderComponent>::ID,
            ComponentID<Components::View>::ID,
            ComponentID<Components::Camera>::ID,
            ComponentID<Components::Light>::ID,
            ComponentID<Components::DebugRenderComponent>::ID};
}
} // namespace ECS

#define C_ID(compName) ECS::ComponentID<ECS::Components::compName>::ID